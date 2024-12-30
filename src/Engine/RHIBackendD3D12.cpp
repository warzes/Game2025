#include "stdafx.h"
#if RENDER_D3D12
#include "RHIBackendD3D12.h"
#include "WindowData.h"
#include "Log.h"
#include "RenderCore.h"
#include "TempD3D12.h"
//=============================================================================
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")
//=============================================================================
extern "C" { __declspec(dllexport) extern const UINT D3D12SDKVersion = 715; }
extern "C" { __declspec(dllexport) extern const char* D3D12SDKPath = ".\\D3D12\\"; }
//=============================================================================
RHIBackend gRHI{};
//=============================================================================
#if RHI_VALIDATION_ENABLED
void D3D12MessageCallbackFunc(D3D12_MESSAGE_CATEGORY Category, D3D12_MESSAGE_SEVERITY Severity, D3D12_MESSAGE_ID ID, LPCSTR pDescription, void* pContext) noexcept
{
	Print(pDescription);
}
#endif // RHI_VALIDATION_ENABLED
//=============================================================================
RHIBackend::~RHIBackend()
{
	assert(!device);
}
//=============================================================================
bool RHIBackend::CreateAPI(const WindowData& wndData, const RenderSystemCreateInfo& createInfo)
{
	frameBufferWidth = wndData.width;
	frameBufferHeight = wndData.height;
	vsync = createInfo.vsync;

	enableDebugLayer();
	if (!createAdapter(createInfo)) return false;
	if (!createDevice())            return false;
	configInfoQueue();
	if (!createAllocator())         return false;

	// TEMP =====================================
	g_CommandQueue = CreateCommandQueue(device, D3D12_COMMAND_LIST_TYPE_DIRECT);
	swapChain = CreateSwapChain(wndData.hwnd, g_CommandQueue, wndData.width, wndData.height, NUM_BACK_BUFFERS, supportFeatures.allowTearing);
	g_CurrentBackBufferIndex = swapChain->GetCurrentBackBufferIndex();

	g_RTVDescriptorHeap = CreateDescriptorHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, NUM_BACK_BUFFERS);
	g_RTVDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	UpdateRenderTargetViews(device, swapChain, g_RTVDescriptorHeap);

	for (int i = 0; i < NUM_BACK_BUFFERS; ++i)
	{
		g_CommandAllocators[i] = CreateCommandAllocator(device, D3D12_COMMAND_LIST_TYPE_DIRECT);
	}
	g_CommandList = CreateCommandList(device,
		g_CommandAllocators[g_CurrentBackBufferIndex], D3D12_COMMAND_LIST_TYPE_DIRECT);

	g_Fence = CreateFence(device);
	g_FenceEvent = CreateEventHandle();

	// TEMP =====================================

	return true;
}
//=============================================================================
void RHIBackend::DestroyAPI()
{
	Flush(g_CommandQueue, g_Fence, g_FenceValue, g_FenceEvent);

	allocator.Reset();
	device.Reset();
	adapter.Reset();

#if defined(_DEBUG)
	ComPtr<IDXGIDebug1> debug;
	if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&debug))))
	{
		debug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_FLAGS(DXGI_DEBUG_RLO_SUMMARY | DXGI_DEBUG_RLO_DETAIL | DXGI_DEBUG_RLO_IGNORE_INTERNAL));
	}
#endif
}
//=============================================================================
void RHIBackend::ResizeFrameBuffer(uint32_t width, uint32_t height)
{
	// Don't allow 0 size swap chain back buffers.
	width = std::max(1u, width);
	height = std::max(1u, height);
	if (frameBufferWidth == width && frameBufferHeight == height) return;

	frameBufferWidth = width;
	frameBufferHeight = height;

	// Flush the GPU queue to make sure the swap chain's back buffers are not being referenced by an in-flight command list.
	Flush(g_CommandQueue, g_Fence, g_FenceValue, g_FenceEvent);

	for (int i = 0; i < NUM_BACK_BUFFERS; ++i)
	{
		// Any references to the back buffers must be released before the swap chain can be resized.
		g_BackBuffers[i].Reset();
		g_FrameFenceValues[i] = g_FrameFenceValues[g_CurrentBackBufferIndex];
	}

	DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
	swapChain->GetDesc(&swapChainDesc);
	swapChain->ResizeBuffers(NUM_BACK_BUFFERS, frameBufferWidth, frameBufferHeight, swapChainDesc.BufferDesc.Format, swapChainDesc.Flags);

	g_CurrentBackBufferIndex = swapChain->GetCurrentBackBufferIndex();

	UpdateRenderTargetViews(device, swapChain, g_RTVDescriptorHeap);
}
//=============================================================================
void RHIBackend::BeginFrame()
{
}
//=============================================================================
void RHIBackend::EndFrame()
{
}
//=============================================================================
void RHIBackend::enableDebugLayer()
{
#if RHI_VALIDATION_ENABLED
	ComPtr<ID3D12Debug> debugController;
	if (FAILED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) return;
	debugController->EnableDebugLayer();

	ComPtr<ID3D12Debug1> debugController1;
	if (FAILED(debugController.As(&debugController1))) return;
	debugController1->SetEnableGPUBasedValidation(TRUE); // TODO: может сильно замедлять работу, поэтому возможно должна быть тонкая настройка
	debugController1->SetEnableSynchronizedCommandQueueValidation(TRUE);

	ComPtr<ID3D12Debug5> debugController5;
	if (FAILED(debugController1.As(&debugController5))) return;
	debugController5->SetEnableAutoName(TRUE);
#endif // RHI_VALIDATION_ENABLED
}
//=============================================================================
bool RHIBackend::createAdapter(const RenderSystemCreateInfo& createInfo)
{
	UINT dxgiFactoryFlags = 0;
#if RHI_VALIDATION_ENABLED
	dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
#endif // RHI_VALIDATION_ENABLED

	ComPtr<IDXGIFactory> DXGIFactory;
	HRESULT result = CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&DXGIFactory));
	if (FAILED(result))
	{
		Fatal("CreateDXGIFactory2() failed: " + DXErrorToStr(result));
		return false;
	}
	ComPtr<IDXGIFactory7> DXGIFactory7;
	result = DXGIFactory.As(&DXGIFactory7);
	if (FAILED(result))
	{
		Fatal("IDXGIFactory As IDXGIFactory7 failed: " + DXErrorToStr(result));
		return false;
	}

	{
		// при отключении vsync на экране могут быть разрывы
		BOOL allowTearing = FALSE;
		if (FAILED(DXGIFactory7->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allowTearing, sizeof(allowTearing))))
			supportFeatures.allowTearing = false;
		else
			supportFeatures.allowTearing = (allowTearing == TRUE);
	}

	ComPtr<IDXGIAdapter> DXGIAdapter;
	if (createInfo.useWarp)
	{
		result = DXGIFactory7->EnumWarpAdapter(IID_PPV_ARGS(&DXGIAdapter));
		if (FAILED(result))
		{
			Fatal("IDXGIFactory7::EnumWarpAdapter() failed: " + DXErrorToStr(result));
			return false;
		}
	}
	else
	{
		result = DXGIFactory7->EnumAdapterByGpuPreference(0, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&DXGIAdapter));
		if (FAILED(result))
		{
			Fatal("IDXGIFactory7::EnumAdapterByGpuPreference() failed: " + DXErrorToStr(result));
			return false;
		}
	}
	result = DXGIAdapter.As(&adapter);
	if (FAILED(result))
	{
		Fatal("DXGIAdapter As DXGIAdapter4 failed: " + DXErrorToStr(result));
		return false;
	}

	return true;
}
//=============================================================================
bool RHIBackend::createDevice()
{
	const D3D_FEATURE_LEVEL featureLevels[] = {
			D3D_FEATURE_LEVEL_12_2,
			D3D_FEATURE_LEVEL_12_1,
			D3D_FEATURE_LEVEL_12_0,
			D3D_FEATURE_LEVEL_11_1,
			D3D_FEATURE_LEVEL_11_0,
	};
	HRESULT result{};
	for (auto& featureLevel : featureLevels)
	{

		result = D3D12CreateDevice(adapter.Get(), featureLevel, IID_PPV_ARGS(&device));
		if (SUCCEEDED(result))
		{
			Print("Direct3D 12 Feature Level: " + ConvertStr(featureLevel));
			break;
		}
	}
	if (FAILED(result))
	{
		Fatal("D3D12CreateDevice() failed: " + DXErrorToStr(result));
		return false;
	}

	return true;
}
//=============================================================================
void RHIBackend::configInfoQueue()
{
#if RHI_VALIDATION_ENABLED
	ComPtr<ID3D12InfoQueue1> infoQueue{ nullptr };
	if (SUCCEEDED(device.As(&infoQueue)))
	{
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
		//infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);

		std::vector<D3D12_MESSAGE_ID> filteredMessages = {
			D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,
			D3D12_MESSAGE_ID_CLEARDEPTHSTENCILVIEW_MISMATCHINGCLEARVALUE,
			D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE, // This warning occurs when using capture frame while graphics debugging.
			D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE
		};
		D3D12_INFO_QUEUE_FILTER filter = {};
		filter.DenyList.NumIDs = (UINT)filteredMessages.size();
		filter.DenyList.pIDList = filteredMessages.data();
		infoQueue->AddStorageFilterEntries(&filter);

		infoQueue->RegisterMessageCallback(D3D12MessageCallbackFunc, D3D12_MESSAGE_CALLBACK_FLAG_NONE, nullptr, nullptr);
	}
#endif // RHI_VALIDATION_ENABLED
}
//=============================================================================
bool RHIBackend::createAllocator()
{
	D3D12MA::ALLOCATOR_DESC desc = {};
	desc.Flags = D3D12MA::ALLOCATOR_FLAG_NONE;
	desc.pDevice = device.Get();
	desc.pAdapter = adapter.Get();

	HRESULT result = D3D12MA::CreateAllocator(&desc, &allocator);
	if (FAILED(result))
	{
		Fatal("CreateAllocator() failed: " + DXErrorToStr(result));
		return false;
	}

	return true;
}
//=============================================================================
#endif // RENDER_D3D12