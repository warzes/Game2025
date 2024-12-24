#include "stdafx.h"
#if RENDER_D3D12
#include "RHIBackend.h"
#include "WindowData.h"
#include "RenderContextD3D12.h"
#include "Log.h"
#include "RenderCoreD3D12.h"
//=============================================================================
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")
//=============================================================================
extern "C" { __declspec(dllexport) extern const UINT D3D12SDKVersion = 715; }
extern "C" { __declspec(dllexport) extern const char* D3D12SDKPath = ".\\D3D12\\"; }
//=============================================================================
RenderContext gRenderContext{};
//=============================================================================
bool CreateWindowDependentResources(HWND windowHandle, glm::ivec2 screenSize)
{
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
	ZeroMemory(&swapChainDesc, sizeof(swapChainDesc));
	swapChainDesc.Width = lround(screenSize.x);
	swapChainDesc.Height = lround(screenSize.y);
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.Stereo = false;
	swapChainDesc.SampleDesc.Count = 1;
	swapChainDesc.SampleDesc.Quality = 0;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.BufferCount = NUM_BACK_BUFFERS;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.Flags = 0;
	swapChainDesc.Scaling = DXGI_SCALING_NONE;
	swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;

	IDXGISwapChain1* swapChain = nullptr;
	HRESULT result = gRenderContext.DXGIFactory->CreateSwapChainForHwnd(gRenderContext.graphicsQueue->GetDeviceQueue(), windowHandle, &swapChainDesc, nullptr, nullptr, &swapChain);
	if (FAILED(result))
	{
		Fatal("IDXGIFactory7::CreateSwapChainForHwnd() failed: " + DXErrorToStr(result));
		return false;
	}

	result = swapChain->QueryInterface(__uuidof(IDXGISwapChain3), (void**)&gRenderContext.swapChain);
	SafeRelease(swapChain);
	if (FAILED(result))
	{
		Fatal("IDXGISwapChain1::QueryInterface() failed: " + DXErrorToStr(result));
		return false;
	}

	for (uint32_t bufferIndex = 0; bufferIndex < NUM_BACK_BUFFERS; bufferIndex++)
	{
		ID3D12Resource* backBufferResource = nullptr;
		Descriptor backBufferRTVHandle = gRenderContext.RTVStagingDescriptorHeap->GetNewDescriptor();

		D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
		rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
		rtvDesc.Texture2D.MipSlice = 0;
		rtvDesc.Texture2D.PlaneSlice = 0;

		result = gRenderContext.swapChain->GetBuffer(bufferIndex, IID_PPV_ARGS(&backBufferResource));
		if (FAILED(result))
		{
			Fatal("IDXGISwapChain4::GetBuffer() failed: " + DXErrorToStr(result));
			return false;
		}

		gRenderContext.device->CreateRenderTargetView(backBufferResource, &rtvDesc, backBufferRTVHandle.CPUHandle);

		gRenderContext.backBuffers[bufferIndex] = new TextureResource();
		gRenderContext.backBuffers[bufferIndex]->desc = backBufferResource->GetDesc();
		gRenderContext.backBuffers[bufferIndex]->resource = backBufferResource;
		gRenderContext.backBuffers[bufferIndex]->state = D3D12_RESOURCE_STATE_PRESENT;
		gRenderContext.backBuffers[bufferIndex]->RTVDescriptor = backBufferRTVHandle;
	}

	gRenderContext.frameId = 0;

	return true;
}
//=============================================================================
void DestroyWindowDependentResources()
{
	for (uint32_t bufferIndex = 0; bufferIndex < NUM_BACK_BUFFERS; bufferIndex++)
	{
		if (gRenderContext.backBuffers[bufferIndex])
		{
			gRenderContext.RTVStagingDescriptorHeap->FreeDescriptor(gRenderContext.backBuffers[bufferIndex]->RTVDescriptor);
			SafeRelease(gRenderContext.backBuffers[bufferIndex]->resource);
			gRenderContext.backBuffers[bufferIndex] = nullptr;
		}
	}

	SafeRelease(gRenderContext.swapChain);
}
//=============================================================================
bool RHIBackend::CreateAPI(const WindowData& wndData, const RenderSystemCreateInfo& createInfo)
{
	gRenderContext.frameBufferWidth = wndData.width;
	gRenderContext.frameBufferHeight = wndData.height;

#if RHI_VALIDATION_ENABLED
	ID3D12Debug* debugController{ nullptr };
	ID3D12Debug6* debugController6{ nullptr };
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
	{
		if (SUCCEEDED(debugController->QueryInterface(IID_PPV_ARGS(&debugController6))))
		{
			debugController6->EnableDebugLayer();
			debugController6->SetEnableGPUBasedValidation(true);
		}
		else
		{
			debugController->EnableDebugLayer();
		}
	}
	SafeRelease(debugController6);
	SafeRelease(debugController);
#endif // RHI_VALIDATION_ENABLED

	UINT dxgiFactoryFlags = 0;
#if RHI_VALIDATION_ENABLED
	dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
#endif // RHI_VALIDATION_ENABLED

	HRESULT result = CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&gRenderContext.DXGIFactory));
	if (FAILED(result))
	{
		Fatal("CreateDXGIFactory2() failed: " + DXErrorToStr(result));
		return false;
	}
	result = gRenderContext.DXGIFactory->EnumAdapterByGpuPreference(0, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&gRenderContext.adapter));
	if (FAILED(result))
	{
		Fatal("IDXGIFactory7::EnumAdapterByGpuPreference() failed: " + DXErrorToStr(result));
		return false;
	}
	const D3D_FEATURE_LEVEL featureLevels[] = {
			D3D_FEATURE_LEVEL_12_2,
			D3D_FEATURE_LEVEL_12_1,
			D3D_FEATURE_LEVEL_12_0,
			D3D_FEATURE_LEVEL_11_1,
			D3D_FEATURE_LEVEL_11_0,
	};
	for (auto& featureLevel : featureLevels)
	{
		result = D3D12CreateDevice(gRenderContext.adapter, featureLevel, IID_PPV_ARGS(&gRenderContext.device));
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

#if RHI_VALIDATION_ENABLED
	ID3D12InfoQueue* infoQueue{ nullptr };
	if (SUCCEEDED(gRenderContext.device->QueryInterface(IID_PPV_ARGS(&infoQueue))))
	{
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
		//infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);

		std::vector<D3D12_MESSAGE_ID> filtered_messages = {
			D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,
			D3D12_MESSAGE_ID_CLEARDEPTHSTENCILVIEW_MISMATCHINGCLEARVALUE,
		D3D12_MESSAGE_ID_DRAW_EMPTY_SCISSOR_RECTANGLE
		};

		D3D12_INFO_QUEUE_FILTER filter = {};
		filter.DenyList.NumIDs = (UINT)filtered_messages.size();
		filter.DenyList.pIDList = filtered_messages.data();
		infoQueue->AddStorageFilterEntries(&filter);
	}
	else
	{
		Error("Get ID3D12InfoQueue failed: " + DXErrorToStr(result));
	}
	SafeRelease(infoQueue);
#endif // RHI_VALIDATION_ENABLED

	D3D12MA::ALLOCATOR_DESC desc = {};
	desc.Flags = D3D12MA::ALLOCATOR_FLAG_NONE;
	desc.pDevice = gRenderContext.device;
	desc.pAdapter = gRenderContext.adapter;

	result = D3D12MA::CreateAllocator(&desc, &gRenderContext.allocator);
	if (FAILED(result))
	{
		Fatal("CreateAllocator() failed: " + DXErrorToStr(result));
		return false;
	}

	gRenderContext.graphicsQueue = new QueueD3D12(gRenderContext.device, D3D12_COMMAND_LIST_TYPE_DIRECT);
	gRenderContext.computeQueue = new QueueD3D12(gRenderContext.device, D3D12_COMMAND_LIST_TYPE_COMPUTE);
	gRenderContext.copyQueue = new QueueD3D12(gRenderContext.device, D3D12_COMMAND_LIST_TYPE_COPY);
	if (IsRequestExit())
	{
		Fatal("Create Queue failed.");
		return false;
	}

	gRenderContext.RTVStagingDescriptorHeap = new StagingDescriptorHeap(gRenderContext.device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, NUM_RTV_STAGING_DESCRIPTORS);
	gRenderContext.DSVStagingDescriptorHeap = new StagingDescriptorHeap(gRenderContext.device, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, NUM_DSV_STAGING_DESCRIPTORS);
	gRenderContext.SRVStagingDescriptorHeap = new StagingDescriptorHeap(gRenderContext.device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, NUM_SRV_STAGING_DESCRIPTORS);
	gRenderContext.SamplerRenderPassDescriptorHeap = new RenderPassDescriptorHeap(gRenderContext.device, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 0, NUM_SAMPLER_DESCRIPTORS);

	for (uint32_t frameIndex = 0; frameIndex < NUM_FRAMES_IN_FLIGHT; frameIndex++)
	{
		gRenderContext.SRVRenderPassDescriptorHeaps[frameIndex] = new RenderPassDescriptorHeap(gRenderContext.device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, NUM_RESERVED_SRV_DESCRIPTORS, NUM_SRV_RENDER_PASS_USER_DESCRIPTORS);
		gRenderContext.ImguiDescriptors[frameIndex] = gRenderContext.SRVRenderPassDescriptorHeaps[frameIndex]->GetReservedDescriptor(IMGUI_RESERVED_DESCRIPTOR_INDEX);
	}
	if (IsRequestExit())
	{
		Fatal("Create DescriptorHeap failed.");
		return false;
	}

	// Create Sampler
	{
		D3D12_SAMPLER_DESC samplerDescs[NUM_SAMPLER_DESCRIPTORS]{};
		samplerDescs[0].Filter = D3D12_FILTER_ANISOTROPIC;
		samplerDescs[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		samplerDescs[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		samplerDescs[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		samplerDescs[0].BorderColor[0] = samplerDescs[0].BorderColor[1] = samplerDescs[0].BorderColor[2] = samplerDescs[0].BorderColor[3] = 0.0f;
		samplerDescs[0].MipLODBias = 0.0f;
		samplerDescs[0].MaxAnisotropy = 16;
		samplerDescs[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NONE;
		samplerDescs[0].MinLOD = 0;
		samplerDescs[0].MaxLOD = D3D12_FLOAT32_MAX;

		samplerDescs[1].Filter = D3D12_FILTER_ANISOTROPIC;
		samplerDescs[1].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		samplerDescs[1].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		samplerDescs[1].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		samplerDescs[1].BorderColor[0] = samplerDescs[1].BorderColor[1] = samplerDescs[1].BorderColor[2] = samplerDescs[1].BorderColor[3] = 0.0f;
		samplerDescs[1].MipLODBias = 0.0f;
		samplerDescs[1].MaxAnisotropy = 16;
		samplerDescs[1].ComparisonFunc = D3D12_COMPARISON_FUNC_NONE;
		samplerDescs[1].MinLOD = 0;
		samplerDescs[1].MaxLOD = D3D12_FLOAT32_MAX;

		samplerDescs[2].Filter = D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR;
		samplerDescs[2].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		samplerDescs[2].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		samplerDescs[2].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		samplerDescs[2].BorderColor[0] = samplerDescs[2].BorderColor[1] = samplerDescs[2].BorderColor[2] = samplerDescs[2].BorderColor[3] = 0.0f;
		samplerDescs[2].MipLODBias = 0.0f;
		samplerDescs[2].MaxAnisotropy = 0;
		samplerDescs[2].ComparisonFunc = D3D12_COMPARISON_FUNC_NONE;
		samplerDescs[2].MinLOD = 0;
		samplerDescs[2].MaxLOD = D3D12_FLOAT32_MAX;

		samplerDescs[3].Filter = D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR;
		samplerDescs[3].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		samplerDescs[3].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		samplerDescs[3].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		samplerDescs[3].BorderColor[0] = samplerDescs[3].BorderColor[1] = samplerDescs[3].BorderColor[2] = samplerDescs[3].BorderColor[3] = 0.0f;
		samplerDescs[3].MipLODBias = 0.0f;
		samplerDescs[3].MaxAnisotropy = 0;
		samplerDescs[3].ComparisonFunc = D3D12_COMPARISON_FUNC_NONE;
		samplerDescs[3].MinLOD = 0;
		samplerDescs[3].MaxLOD = D3D12_FLOAT32_MAX;

		samplerDescs[4].Filter = D3D12_FILTER_COMPARISON_MIN_MAG_MIP_POINT;
		samplerDescs[4].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		samplerDescs[4].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		samplerDescs[4].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		samplerDescs[4].BorderColor[0] = samplerDescs[4].BorderColor[1] = samplerDescs[4].BorderColor[2] = samplerDescs[4].BorderColor[3] = 0.0f;
		samplerDescs[4].MipLODBias = 0.0f;
		samplerDescs[4].MaxAnisotropy = 0;
		samplerDescs[4].ComparisonFunc = D3D12_COMPARISON_FUNC_NONE;
		samplerDescs[4].MinLOD = 0;
		samplerDescs[4].MaxLOD = D3D12_FLOAT32_MAX;

		samplerDescs[5].Filter = D3D12_FILTER_COMPARISON_MIN_MAG_MIP_POINT;
		samplerDescs[5].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		samplerDescs[5].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		samplerDescs[5].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		samplerDescs[5].BorderColor[0] = samplerDescs[5].BorderColor[1] = samplerDescs[5].BorderColor[2] = samplerDescs[5].BorderColor[3] = 0.0f;
		samplerDescs[5].MipLODBias = 0.0f;
		samplerDescs[5].MaxAnisotropy = 0;
		samplerDescs[5].ComparisonFunc = D3D12_COMPARISON_FUNC_NONE;
		samplerDescs[5].MinLOD = 0;
		samplerDescs[5].MaxLOD = D3D12_FLOAT32_MAX;

		Descriptor samplerDescriptorBlock = gRenderContext.SamplerRenderPassDescriptorHeap->AllocateUserDescriptorBlock(NUM_SAMPLER_DESCRIPTORS);
		D3D12_CPU_DESCRIPTOR_HANDLE currentSamplerDescriptor = samplerDescriptorBlock.CPUHandle;

		for (uint32_t samplerIndex = 0; samplerIndex < NUM_SAMPLER_DESCRIPTORS; samplerIndex++)
		{
			gRenderContext.device->CreateSampler(&samplerDescs[samplerIndex], currentSamplerDescriptor);
			currentSamplerDescriptor.ptr += gRenderContext.SamplerRenderPassDescriptorHeap->GetDescriptorSize();
		}
	}

	BufferCreationDesc uploadBufferDesc;
	uploadBufferDesc.size = 10 * 1024 * 1024;
	uploadBufferDesc.accessFlags = BufferAccessFlags::hostWritable;

	BufferCreationDesc uploadTextureDesc;
	uploadTextureDesc.size = 40 * 1024 * 1024;
	uploadTextureDesc.accessFlags = BufferAccessFlags::hostWritable;

	for (uint32_t frameIndex = 0; frameIndex < NUM_FRAMES_IN_FLIGHT; frameIndex++)
	{
		gRenderContext.uploadContexts[frameIndex] = new UploadContext(CreateBuffer(uploadBufferDesc), CreateBuffer(uploadTextureDesc));
	}

	//The -1 and starting at index 1 accounts for the imgui descriptor.
	gRenderContext.FreeReservedDescriptorIndices.resize(NUM_RESERVED_SRV_DESCRIPTORS - 1);
	std::iota(gRenderContext.FreeReservedDescriptorIndices.begin(), gRenderContext.FreeReservedDescriptorIndices.end(), 1);

	if (!CreateWindowDependentResources(wndData.hwnd, { wndData.width, wndData.height })) return false;

	return true;
}
//=============================================================================
void RHIBackend::DestroyAPI()
{
	WaitForIdle();
	DestroyWindowDependentResources();
	gRenderContext.Release();
}
//=============================================================================
void RHIBackend::ResizeFrameBuffer(uint32_t width, uint32_t height)
{
	if (gRenderContext.frameBufferWidth == width && gRenderContext.frameBufferHeight == height) return;
	
	gRenderContext.frameBufferWidth = width;
	gRenderContext.frameBufferHeight = height;
}
//=============================================================================
void RHIBackend::BeginFrame()
{
	gRenderContext.frameId = (gRenderContext.frameId + 1) % NUM_FRAMES_IN_FLIGHT;

	//wait on fences from 2 frames ago
	gRenderContext.graphicsQueue->WaitForFenceCPUBlocking(gRenderContext.endOfFrameFences[gRenderContext.frameId].graphicsQueueFence);
	gRenderContext.computeQueue->WaitForFenceCPUBlocking(gRenderContext.endOfFrameFences[gRenderContext.frameId].computeQueueFence);
	gRenderContext.copyQueue->WaitForFenceCPUBlocking(gRenderContext.endOfFrameFences[gRenderContext.frameId].copyQueueFence);

	ProcessDestructions(gRenderContext.frameId);

	gRenderContext.uploadContexts[gRenderContext.frameId]->ResolveProcessedUploads();
	gRenderContext.uploadContexts[gRenderContext.frameId]->Reset();

	gRenderContext.contextSubmissions[gRenderContext.frameId].clear();
}
//=============================================================================
void RHIBackend::EndFrame()
{
	gRenderContext.uploadContexts[gRenderContext.frameId]->ProcessUploads();
	SubmitContextWork(*gRenderContext.uploadContexts[gRenderContext.frameId]);

	gRenderContext.endOfFrameFences[gRenderContext.frameId].computeQueueFence = gRenderContext.computeQueue->SignalFence();
	gRenderContext.endOfFrameFences[gRenderContext.frameId].copyQueueFence = gRenderContext.copyQueue->SignalFence();
}
//=============================================================================
void RHIBackend::Present()
{
	gRenderContext.swapChain->Present(0, 0);
	gRenderContext.endOfFrameFences[gRenderContext.frameId].graphicsQueueFence = gRenderContext.graphicsQueue->SignalFence();
}
//=============================================================================
#endif // RENDER_D3D12