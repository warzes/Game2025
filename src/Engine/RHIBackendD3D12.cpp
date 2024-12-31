#include "stdafx.h"
#if RENDER_D3D12
#include "RHIBackendD3D12.h"
#include "WindowData.h"
#include "Log.h"
#include "RenderCore.h"
//=============================================================================
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")
//=============================================================================
extern "C" { __declspec(dllexport) extern const UINT D3D12SDKVersion = D3D12_PREVIEW_SDK_VERSION; }
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
	setSize(wndData.width, wndData.height);
	vsync = createInfo.vsync;

	enableDebugLayer(createInfo);
	if (!selectAdapter(createInfo)) return false;
	if (!createDevice())            return false;
	if (!createAllocator())         return false;

	commandQueue = createCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT);
	if (!commandQueue) return false;

	if (!createSwapChain(wndData))  return false;

	// Create descriptor heaps for render target views and depth stencil views.
	rtvDescriptorHeap = createDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, MAX_BACK_BUFFER_COUNT);
	if (!rtvDescriptorHeap) return false;
	rtvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	if (depthBufferFormat != DXGI_FORMAT_UNKNOWN)
	{
		dsvDescriptorHeap = createDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1);
		if (!dsvDescriptorHeap) return false;
	}

	// Create a command allocator for each back buffer that will be rendered to.
	for (int i = 0; i < MAX_BACK_BUFFER_COUNT; ++i)
	{
		HRESULT result = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocators[i]));
		if (FAILED(result))
		{
			Fatal("ID3D12Device14::CreateCommandAllocator() failed: " + DXErrorToStr(result));
			return false;
		}
		wchar_t name[25] = {};
		swprintf_s(name, L"Render target %u", i);
		commandAllocators[i]->SetName(name);
	}

	// Create a command list for recording graphics commands.
	HRESULT result = device->CreateCommandList1(0, D3D12_COMMAND_LIST_TYPE_DIRECT, D3D12_COMMAND_LIST_FLAG_NONE, IID_PPV_ARGS(&commandList));
	if (FAILED(result))
	{
		Fatal("ID3D12Device14::CreateCommandList1() failed: " + DXErrorToStr(result));
		return false;
	}

	// Create a fence for tracking GPU execution progress.
	result = device->CreateFence(fenceValues[currentBackBufferIndex], D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
	if (FAILED(result))
	{
		Fatal("ID3D12Device14::CreateFence() failed: " + DXErrorToStr(result));
		return false;
	}
	fenceValues[currentBackBufferIndex]++;

	fenceEvent.Attach(CreateEventEx(nullptr, nullptr, 0, EVENT_MODIFY_STATE | SYNCHRONIZE));
	if (!fenceEvent.IsValid())
	{
		Fatal("CreateEventEx() failed.");
		return false;
	}

	if (!updateRenderTargetViews()) return false;

	return true;
}
//=============================================================================
void RHIBackend::DestroyAPI()
{
	WaitForGpu();

	for (size_t i = 0; i < MAX_BACK_BUFFER_COUNT; i++)
	{
		fenceValues[i] = 0;
		commandAllocators[i].Reset();
		backBuffers[i].Reset();
	}
	commandList.Reset();
	commandQueue.Reset();
	fence.Reset();
	fenceEvent.Close();
	depthStencil.Reset();
	rtvDescriptorHeap.Reset();
	dsvDescriptorHeap.Reset();

	rtvDescriptorSize = 0;
	currentBackBufferIndex = 0;
	frameBufferWidth = 0;
	frameBufferHeight = 0;

	swapChain.Reset();
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
	if (!setSize(width, height)) return;

	// Wait until all previous GPU work is complete.
	WaitForGpu();
	DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
	HRESULT result = swapChain->GetDesc(&swapChainDesc);
	if (FAILED(result))
	{
		Fatal("IDXGISwapChain4::GetDesc() failed: " + DXErrorToStr(result));
		return;
	}

	for (UINT n = 0; n < MAX_BACK_BUFFER_COUNT; n++)
	{
		backBuffers[n].Reset();
	}

	result = swapChain->ResizeBuffers(MAX_BACK_BUFFER_COUNT, frameBufferWidth, frameBufferHeight, swapChainDesc.BufferDesc.Format, swapChainDesc.Flags);
	if (FAILED(result))
	{
		Fatal("IDXGISwapChain4::ResizeBuffers() failed: " + DXErrorToStr(result));
		return;
	}
	if (!updateRenderTargetViews()) return;
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
void RHIBackend::Prepare(D3D12_RESOURCE_STATES beforeState, D3D12_RESOURCE_STATES afterState)
{
	// Reset command list and allocator.
	HRESULT result = commandAllocators[currentBackBufferIndex]->Reset();
	if (FAILED(result))
	{
		Fatal("ID3D12CommandAllocator::Reset() failed: " + DXErrorToStr(result));
		return;
	}

	result = commandList->Reset(commandAllocators[currentBackBufferIndex].Get(), nullptr);
	if (FAILED(result))
	{
		Fatal("ID3D12GraphicsCommandList10::Reset() failed: " + DXErrorToStr(result));
		return;
	}

	if (beforeState != afterState)
	{
		// Transition the render target into the correct state to allow for drawing into it.
		const D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
			backBuffers[currentBackBufferIndex].Get(),
			beforeState, afterState);
		// TODO: по хорошему нужно собрать все барьеры и применить их за раз
		commandList->ResourceBarrier(1, &barrier);
	}
}
//=============================================================================
void RHIBackend::Present(D3D12_RESOURCE_STATES beforeState)
{
	if (beforeState != D3D12_RESOURCE_STATE_PRESENT)
	{
		// Transition the render target to the state that allows it to be presented to the display.
		const D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
			backBuffers[currentBackBufferIndex].Get(),
			beforeState, D3D12_RESOURCE_STATE_PRESENT);
		commandList->ResourceBarrier(1, &barrier);
	}

	// Send the command list off to the GPU for processing.
	HRESULT result = commandList->Close();
	if (FAILED(result))
	{
		Fatal("ID3D12GraphicsCommandList10::Close() failed: " + DXErrorToStr(result));
		return;
	}
	commandQueue->ExecuteCommandLists(1, CommandListCast(commandList.GetAddressOf()));

	UINT syncInterval = vsync ? 1 : 0;
	UINT presentFlags = supportFeatures.allowTearing && !vsync ? DXGI_PRESENT_ALLOW_TEARING : 0;
	result = swapChain->Present(syncInterval, presentFlags);
	if (FAILED(result))
	{
		Fatal("IDXGISwapChain4::Present() failed: " + DXErrorToStr(result));
		return;
	}

	moveToNextFrame();
}
//=============================================================================
void RHIBackend::WaitForGpu()
{
	if (commandQueue && fence && fenceEvent.IsValid())
	{
		// Schedule a Signal command in the GPU queue.
		const UINT64 fenceValue = fenceValues[currentBackBufferIndex];
		if (SUCCEEDED(commandQueue->Signal(fence.Get(), fenceValue)))
		{
			// Wait until the Signal has been processed.
			if (SUCCEEDED(fence->SetEventOnCompletion(fenceValue, fenceEvent.Get())))
			{
				std::ignore = WaitForSingleObjectEx(fenceEvent.Get(), INFINITE, FALSE);

				// Increment the fence value for the current frame.
				fenceValues[currentBackBufferIndex]++;
			}
		}
	}
}
//=============================================================================
bool RHIBackend::setSize(uint32_t width, uint32_t height)
{
	// Don't allow 0 size swap chain back buffers.
	width = std::max(1u, width);
	height = std::max(1u, height);
	if (frameBufferWidth == width && frameBufferHeight == height) return false;
	frameBufferWidth = width;
	frameBufferHeight = height;

	return true;
}
//=============================================================================
void RHIBackend::enableDebugLayer(const RenderSystemCreateInfo& createInfo)
{
#if RHI_VALIDATION_ENABLED
	ComPtr<IDXGIInfoQueue> dxgiInfoQueue;
	if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiInfoQueue))))
	{
		dxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR, true);
		dxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_CORRUPTION, true);

		DXGI_INFO_QUEUE_MESSAGE_ID hide[] =
		{
			80 /* IDXGISwapChain::GetContainingOutput: The swapchain's adapter does not control the output on which the swapchain's window resides. */,
		};
		DXGI_INFO_QUEUE_FILTER filter = {};
		filter.DenyList.NumIDs = static_cast<UINT>(std::size(hide));
		filter.DenyList.pIDList = hide;
		dxgiInfoQueue->AddStorageFilterEntries(DXGI_DEBUG_DXGI, &filter);
	}

	ComPtr<ID3D12Debug> debugController;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
	{
		debugController->EnableDebugLayer();
		Print("Direct3D 12 Enable Debug Layer");
	}
	else
	{
		Warning("Direct3D Debug Device is not available");
		return;
	}

	ComPtr<ID3D12Debug1> debugController1;
	if (FAILED(debugController.As(&debugController1))) return;
	if (createInfo.debug.enableGPUBasedValidation)
	{
		debugController1->SetEnableGPUBasedValidation(TRUE);
		Print("Direct3D 12 Enable Debug GPU Based Validation");
	}
	if (createInfo.debug.enableSynchronizedCommandQueueValidation)
	{
		debugController1->SetEnableSynchronizedCommandQueueValidation(TRUE);
		Print("Direct3D 12 Enable Debug Synchronized Command Queue Validation");
	}

	ComPtr<ID3D12Debug5> debugController5;
	if (FAILED(debugController1.As(&debugController5))) return;
	if (createInfo.debug.enableAutoName)
	{
		debugController5->SetEnableAutoName(TRUE);
		Print("Direct3D 12 Enable Debug Auto Name");
	}
#endif // RHI_VALIDATION_ENABLED
}
//=============================================================================
bool RHIBackend::selectAdapter(const RenderSystemCreateInfo& createInfo)
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
			Print("Direct3D 12 Feature Level: " + ConvertToStr(featureLevel));
			break;
		}
	}
	if (FAILED(result))
	{
		Fatal("D3D12CreateDevice() failed: " + DXErrorToStr(result));
		return false;
	}

	device->SetName(L"MiniEngine");

#if RHI_VALIDATION_ENABLED
	ComPtr<ID3D12InfoQueue1> d3dInfoQueue;
	if (SUCCEEDED(device.As(&d3dInfoQueue)))
	{
		d3dInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
		d3dInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
		//d3dInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);

		D3D12_MESSAGE_ID filteredMessages[] = {
			//D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,
			//D3D12_MESSAGE_ID_CLEARDEPTHSTENCILVIEW_MISMATCHINGCLEARVALUE,
			D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE, // This warning occurs when using capture frame while graphics debugging.
			D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE,
			D3D12_MESSAGE_ID_EXECUTECOMMANDLISTS_WRONGSWAPCHAINBUFFERREFERENCE,
			D3D12_MESSAGE_ID_RESOURCE_BARRIER_MISMATCHING_COMMAND_LIST_TYPE
		};
		D3D12_INFO_QUEUE_FILTER filter = {};
		filter.DenyList.NumIDs = static_cast<UINT>(std::size(filteredMessages));
		filter.DenyList.pIDList = filteredMessages;
		d3dInfoQueue->AddStorageFilterEntries(&filter);

		d3dInfoQueue->RegisterMessageCallback(D3D12MessageCallbackFunc, D3D12_MESSAGE_CALLBACK_FLAG_NONE, nullptr, nullptr);
	}
#endif // RHI_VALIDATION_ENABLED

	return true;
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
bool RHIBackend::createSwapChain(const WindowData& wndData)
{
	ComPtr<IDXGIFactory2> DXGIFactory;
	HRESULT result = adapter->GetParent(IID_PPV_ARGS(&DXGIFactory));
	if (FAILED(result))
	{
		Fatal("IDXGIAdapter4::GetParent() failed: " + DXErrorToStr(result));
		return false;
	}

	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
	swapChainDesc.Width                 = wndData.width;
	swapChainDesc.Height                = wndData.height;
	swapChainDesc.Format                = backBufferFormat;
	swapChainDesc.Stereo                = FALSE;
	swapChainDesc.SampleDesc            = { 1, 0 };
	swapChainDesc.BufferUsage           = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.BufferCount           = MAX_BACK_BUFFER_COUNT;
	swapChainDesc.Scaling               = DXGI_SCALING_STRETCH;
	swapChainDesc.SwapEffect            = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.AlphaMode             = DXGI_ALPHA_MODE_IGNORE;
	swapChainDesc.Flags = supportFeatures.allowTearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

	DXGI_SWAP_CHAIN_FULLSCREEN_DESC fsSwapChainDesc = {};
	fsSwapChainDesc.Windowed = TRUE;

	ComPtr<IDXGISwapChain1> swapChain1;
	result = DXGIFactory->CreateSwapChainForHwnd(commandQueue.Get(), wndData.hwnd, &swapChainDesc, &fsSwapChainDesc, nullptr, &swapChain1);
	if (FAILED(result))
	{
		Fatal("IDXGIFactory2::CreateSwapChainForHwnd() failed: " + DXErrorToStr(result));
		return false;
	}

	result = DXGIFactory->MakeWindowAssociation(wndData.hwnd, DXGI_MWA_NO_ALT_ENTER);
	if (FAILED(result))
	{
		Fatal("IDXGIFactory2::MakeWindowAssociation() failed: " + DXErrorToStr(result));
		return false;
	}

	result = swapChain1.As(&swapChain);
	if (FAILED(result))
	{
		Fatal("IDXGISwapChain1::QueryInterface() failed: " + DXErrorToStr(result));
		return false;
	}

	currentBackBufferIndex = swapChain->GetCurrentBackBufferIndex();

	return true;
}
//=============================================================================
bool RHIBackend::updateRenderTargetViews()
{
	// Wait until all previous GPU work is complete.
	WaitForGpu();

	// Release resources that are tied to the swap chain and update fence values.
	for (UINT n = 0; n < MAX_BACK_BUFFER_COUNT; n++)
	{
		fenceValues[n] = fenceValues[currentBackBufferIndex];
	}

	// Obtain the back buffers for this window which will be the final render targets and create render target views for each of them.
	for (uint32_t i = 0; i < MAX_BACK_BUFFER_COUNT; ++i)
	{
		HRESULT result = swapChain->GetBuffer(i, IID_PPV_ARGS(backBuffers[i].ReleaseAndGetAddressOf()));
		if (FAILED(result))
		{
			Fatal("IDXGISwapChain4::GetBuffer() failed: " + DXErrorToStr(result));
			return false;
		}

		wchar_t name[25] = {};
		swprintf_s(name, L"Render target %u", i);
		backBuffers[i]->SetName(name);

		D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
		rtvDesc.Format                        = backBufferFormat;
		rtvDesc.ViewDimension                 = D3D12_RTV_DIMENSION_TEXTURE2D;

		const auto cpuHandle = rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
		const CD3DX12_CPU_DESCRIPTOR_HANDLE rtvDescriptor(cpuHandle, static_cast<INT>(i), rtvDescriptorSize);
		device->CreateRenderTargetView(backBuffers[i].Get(), &rtvDesc, rtvDescriptor);
	}

	// Reset the index to the current back buffer.
	currentBackBufferIndex = swapChain->GetCurrentBackBufferIndex();

	if (depthBufferFormat != DXGI_FORMAT_UNKNOWN)
	{
		// Allocate a 2-D surface as the depth/stencil buffer and create a depth/stencil view on this surface.
		const CD3DX12_HEAP_PROPERTIES depthHeapProperties(D3D12_HEAP_TYPE_DEFAULT);

		const D3D12_RESOURCE_DESC depthStencilDesc = CD3DX12_RESOURCE_DESC::Tex2D(
			depthBufferFormat, frameBufferWidth, frameBufferHeight, 1, 0, 1, 0,
			D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL
		);

		const CD3DX12_CLEAR_VALUE depthOptimizedClearValue(depthBufferFormat, 1.0f, 0u);

		HRESULT result = device->CreateCommittedResource(
			&depthHeapProperties,
			D3D12_HEAP_FLAG_NONE,
			&depthStencilDesc,
			D3D12_RESOURCE_STATE_DEPTH_WRITE,
			&depthOptimizedClearValue,
			IID_PPV_ARGS(depthStencil.ReleaseAndGetAddressOf())
		);
		if (FAILED(result))
		{
			Fatal("ID3D12Device14::CreateCommittedResource() failed: " + DXErrorToStr(result));
			return false;
		}

		depthStencil->SetName(L"Depth stencil");

		D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
		dsvDesc.Format                        = depthBufferFormat;
		dsvDesc.ViewDimension                 = D3D12_DSV_DIMENSION_TEXTURE2D;

		const auto cpuHandle = dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
		device->CreateDepthStencilView(depthStencil.Get(), &dsvDesc, cpuHandle);
	}

	// Set the 3D rendering viewport and scissor rectangle to target the entire window.
	screenViewport.TopLeftX = screenViewport.TopLeftY = 0.f;
	screenViewport.Width = static_cast<float>(frameBufferWidth);
	screenViewport.Height = static_cast<float>(frameBufferHeight);
	screenViewport.MinDepth = D3D12_MIN_DEPTH;
	screenViewport.MaxDepth = D3D12_MAX_DEPTH;

	scissorRect.left = scissorRect.top = 0;
	scissorRect.right = static_cast<LONG>(frameBufferWidth);
	scissorRect.bottom = static_cast<LONG>(frameBufferHeight);

	return true;
}
//=============================================================================
ComPtr<ID3D12CommandQueue> RHIBackend::createCommandQueue(D3D12_COMMAND_LIST_TYPE type)
{
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Type                     = type;
	queueDesc.Priority                 = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	queueDesc.Flags                    = D3D12_COMMAND_QUEUE_FLAG_NONE;
	queueDesc.NodeMask                 = 0;
	ComPtr<ID3D12CommandQueue> d3d12CommandQueue;
	HRESULT result = device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&d3d12CommandQueue));
	if (FAILED(result))
	{
		Fatal("ID3D12Device14::CreateCommandQueue() failed: " + DXErrorToStr(result));
		return nullptr;
	}
	return d3d12CommandQueue;
}
//=============================================================================
ComPtr<ID3D12DescriptorHeap> RHIBackend::createDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t numDescriptors)
{
	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	desc.Type                       = type;
	desc.NumDescriptors             = numDescriptors;
	desc.Flags                      = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	desc.NodeMask                   = 0;

	ComPtr<ID3D12DescriptorHeap> descriptorHeap;
	HRESULT result = device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&descriptorHeap));
	if (FAILED(result))
	{
		Fatal("ID3D12Device14::CreateDescriptorHeap() failed: " + DXErrorToStr(result));
		return nullptr;
	}
	return descriptorHeap;
}
//=============================================================================
void RHIBackend::moveToNextFrame()
{
	// Schedule a Signal command in the queue.
	const UINT64 currentFenceValue = fenceValues[currentBackBufferIndex];
	HRESULT result = commandQueue->Signal(fence.Get(), currentFenceValue);
	if (FAILED(result))
	{
		Fatal("ID3D12CommandQueue::Signal() failed: " + DXErrorToStr(result));
		return;
	}

	// Update the back buffer index.
	currentBackBufferIndex = swapChain->GetCurrentBackBufferIndex();

	// If the next frame is not ready to be rendered yet, wait until it is ready.
	if (fence->GetCompletedValue() < fenceValues[currentBackBufferIndex])
	{
		result = fence->SetEventOnCompletion(fenceValues[currentBackBufferIndex], fenceEvent.Get());
		if (FAILED(result))
		{
			Fatal("ID3D12Fence::SetEventOnCompletion() failed: " + DXErrorToStr(result));
			return;
		}

		std::ignore = WaitForSingleObjectEx(fenceEvent.Get(), INFINITE, FALSE);
	}

	// Set the fence value for the next frame.
	fenceValues[currentBackBufferIndex] = currentFenceValue + 1;
}
//=============================================================================
#endif // RENDER_D3D12