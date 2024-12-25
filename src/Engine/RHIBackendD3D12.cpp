#include "stdafx.h"
#if RENDER_D3D12
#include "RHIBackendD3D12.h"
#include "WindowData.h"
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
RHIBackend gRHI{};
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
	HRESULT result = gRHI.DXGIFactory->CreateSwapChainForHwnd(gRHI.graphicsQueue->GetDeviceQueue(), windowHandle, &swapChainDesc, nullptr, nullptr, &swapChain);
	if (FAILED(result))
	{
		Fatal("IDXGIFactory7::CreateSwapChainForHwnd() failed: " + DXErrorToStr(result));
		return false;
	}

	result = swapChain->QueryInterface(__uuidof(IDXGISwapChain3), (void**)&gRHI.swapChain);
	SafeRelease(swapChain);
	if (FAILED(result))
	{
		Fatal("IDXGISwapChain1::QueryInterface() failed: " + DXErrorToStr(result));
		return false;
	}

	for (uint32_t bufferIndex = 0; bufferIndex < NUM_BACK_BUFFERS; bufferIndex++)
	{
		ID3D12Resource* backBufferResource = nullptr;
		Descriptor backBufferRTVHandle = gRHI.RTVStagingDescriptorHeap->GetNewDescriptor();

		D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
		rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
		rtvDesc.Texture2D.MipSlice = 0;
		rtvDesc.Texture2D.PlaneSlice = 0;

		result = gRHI.swapChain->GetBuffer(bufferIndex, IID_PPV_ARGS(&backBufferResource));
		if (FAILED(result))
		{
			Fatal("IDXGISwapChain4::GetBuffer() failed: " + DXErrorToStr(result));
			return false;
		}

		gRHI.device->CreateRenderTargetView(backBufferResource, &rtvDesc, backBufferRTVHandle.CPUHandle);

		gRHI.backBuffers[bufferIndex] = new TextureResource();
		gRHI.backBuffers[bufferIndex]->desc = backBufferResource->GetDesc();
		gRHI.backBuffers[bufferIndex]->resource = backBufferResource;
		gRHI.backBuffers[bufferIndex]->state = D3D12_RESOURCE_STATE_PRESENT;
		gRHI.backBuffers[bufferIndex]->RTVDescriptor = backBufferRTVHandle;
	}

	gRHI.frameId = 0;

	return true;
}
//=============================================================================
void DestroyWindowDependentResources()
{
	for (uint32_t bufferIndex = 0; bufferIndex < NUM_BACK_BUFFERS; bufferIndex++)
	{
		if (gRHI.backBuffers[bufferIndex])
		{
			gRHI.RTVStagingDescriptorHeap->FreeDescriptor(gRHI.backBuffers[bufferIndex]->RTVDescriptor);
			SafeRelease(gRHI.backBuffers[bufferIndex]->resource);
			gRHI.backBuffers[bufferIndex] = nullptr;
		}
	}

	SafeRelease(gRHI.swapChain);
}
//=============================================================================
void CopySRVHandleToReservedTable(Descriptor srvHandle, uint32_t index)
{
	for (uint32_t frameIndex = 0; frameIndex < NUM_FRAMES_IN_FLIGHT; frameIndex++)
	{
		Descriptor targetDescriptor = gRHI.SRVRenderPassDescriptorHeaps[frameIndex]->GetReservedDescriptor(index);

		CopyDescriptorsSimple(1, targetDescriptor.CPUHandle, srvHandle.CPUHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	}
}
//=============================================================================
RHIBackend::~RHIBackend()
{
	assert(!device);
}
//=============================================================================
bool RHIBackend::CreateAPI(const WindowData& wndData, const RenderSystemCreateInfo& createInfo)
{
	gRHI.frameBufferWidth = wndData.width;
	gRHI.frameBufferHeight = wndData.height;

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

	HRESULT result = CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&gRHI.DXGIFactory));
	if (FAILED(result))
	{
		Fatal("CreateDXGIFactory2() failed: " + DXErrorToStr(result));
		return false;
	}
	result = gRHI.DXGIFactory->EnumAdapterByGpuPreference(0, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&gRHI.adapter));
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
		result = D3D12CreateDevice(gRHI.adapter, featureLevel, IID_PPV_ARGS(&gRHI.device));
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
	if (SUCCEEDED(gRHI.device->QueryInterface(IID_PPV_ARGS(&infoQueue))))
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
	desc.pDevice = gRHI.device;
	desc.pAdapter = gRHI.adapter;

	result = D3D12MA::CreateAllocator(&desc, &gRHI.allocator);
	if (FAILED(result))
	{
		Fatal("CreateAllocator() failed: " + DXErrorToStr(result));
		return false;
	}

	gRHI.graphicsQueue = new QueueD3D12(gRHI.device, D3D12_COMMAND_LIST_TYPE_DIRECT);
	gRHI.computeQueue = new QueueD3D12(gRHI.device, D3D12_COMMAND_LIST_TYPE_COMPUTE);
	gRHI.copyQueue = new QueueD3D12(gRHI.device, D3D12_COMMAND_LIST_TYPE_COPY);
	if (IsRequestExit())
	{
		Fatal("Create Queue failed.");
		return false;
	}

	gRHI.RTVStagingDescriptorHeap = new StagingDescriptorHeap(gRHI.device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, NUM_RTV_STAGING_DESCRIPTORS);
	gRHI.DSVStagingDescriptorHeap = new StagingDescriptorHeap(gRHI.device, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, NUM_DSV_STAGING_DESCRIPTORS);
	gRHI.SRVStagingDescriptorHeap = new StagingDescriptorHeap(gRHI.device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, NUM_SRV_STAGING_DESCRIPTORS);
	gRHI.SamplerRenderPassDescriptorHeap = new RenderPassDescriptorHeap(gRHI.device, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 0, NUM_SAMPLER_DESCRIPTORS);

	for (uint32_t frameIndex = 0; frameIndex < NUM_FRAMES_IN_FLIGHT; frameIndex++)
	{
		gRHI.SRVRenderPassDescriptorHeaps[frameIndex] = new RenderPassDescriptorHeap(gRHI.device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, NUM_RESERVED_SRV_DESCRIPTORS, NUM_SRV_RENDER_PASS_USER_DESCRIPTORS);
		gRHI.ImguiDescriptors[frameIndex] = gRHI.SRVRenderPassDescriptorHeaps[frameIndex]->GetReservedDescriptor(IMGUI_RESERVED_DESCRIPTOR_INDEX);
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

		Descriptor samplerDescriptorBlock = gRHI.SamplerRenderPassDescriptorHeap->AllocateUserDescriptorBlock(NUM_SAMPLER_DESCRIPTORS);
		D3D12_CPU_DESCRIPTOR_HANDLE currentSamplerDescriptor = samplerDescriptorBlock.CPUHandle;

		for (uint32_t samplerIndex = 0; samplerIndex < NUM_SAMPLER_DESCRIPTORS; samplerIndex++)
		{
			gRHI.device->CreateSampler(&samplerDescs[samplerIndex], currentSamplerDescriptor);
			currentSamplerDescriptor.ptr += gRHI.SamplerRenderPassDescriptorHeap->GetDescriptorSize();
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
		gRHI.uploadContexts[frameIndex] = new UploadContext(CreateBuffer(uploadBufferDesc), CreateBuffer(uploadTextureDesc));
	}

	//The -1 and starting at index 1 accounts for the imgui descriptor.
	gRHI.FreeReservedDescriptorIndices.resize(NUM_RESERVED_SRV_DESCRIPTORS - 1);
	std::iota(gRHI.FreeReservedDescriptorIndices.begin(), gRHI.FreeReservedDescriptorIndices.end(), 1);

	if (!CreateWindowDependentResources(wndData.hwnd, { wndData.width, wndData.height })) return false;

	return true;
}
//=============================================================================
void RHIBackend::DestroyAPI()
{
	WaitForIdle();
	DestroyWindowDependentResources();
	release();
}
//=============================================================================
void RHIBackend::ResizeFrameBuffer(uint32_t width, uint32_t height)
{
	if (gRHI.frameBufferWidth == width && gRHI.frameBufferHeight == height) return;
	
	gRHI.frameBufferWidth = width;
	gRHI.frameBufferHeight = height;
}
//=============================================================================
void RHIBackend::BeginFrame()
{
	gRHI.frameId = (gRHI.frameId + 1) % NUM_FRAMES_IN_FLIGHT;

	//wait on fences from 2 frames ago
	gRHI.graphicsQueue->WaitForFenceCPUBlocking(gRHI.endOfFrameFences[gRHI.frameId].graphicsQueueFence);
	gRHI.computeQueue->WaitForFenceCPUBlocking(gRHI.endOfFrameFences[gRHI.frameId].computeQueueFence);
	gRHI.copyQueue->WaitForFenceCPUBlocking(gRHI.endOfFrameFences[gRHI.frameId].copyQueueFence);

	ProcessDestructions(gRHI.frameId);

	gRHI.uploadContexts[gRHI.frameId]->ResolveProcessedUploads();
	gRHI.uploadContexts[gRHI.frameId]->Reset();

	gRHI.contextSubmissions[gRHI.frameId].clear();
}
//=============================================================================
void RHIBackend::EndFrame()
{
	gRHI.uploadContexts[gRHI.frameId]->ProcessUploads();
	SubmitContextWork(*gRHI.uploadContexts[gRHI.frameId]);

	gRHI.endOfFrameFences[gRHI.frameId].computeQueueFence = gRHI.computeQueue->SignalFence();
	gRHI.endOfFrameFences[gRHI.frameId].copyQueueFence = gRHI.copyQueue->SignalFence();
}
//=============================================================================
void RHIBackend::Present()
{
	gRHI.swapChain->Present(0, 0);
	gRHI.endOfFrameFences[gRHI.frameId].graphicsQueueFence = gRHI.graphicsQueue->SignalFence();
}
//=============================================================================
TextureResource& RHIBackend::GetCurrentBackBuffer()
{
	return *backBuffers[swapChain->GetCurrentBackBufferIndex()];
}
//=============================================================================
void RHIBackend::release()
{
	for (uint32_t frameIndex = 0; frameIndex < NUM_FRAMES_IN_FLIGHT; frameIndex++)
	{
		if (uploadContexts[frameIndex])
		{
			DestroyBuffer(uploadContexts[frameIndex]->ReturnBufferHeap());
			DestroyBuffer(uploadContexts[frameIndex]->ReturnTextureHeap());
		}
	}

	for (uint32_t frameIndex = 0; frameIndex < NUM_FRAMES_IN_FLIGHT; frameIndex++)
	{
		ProcessDestructions(frameIndex);
	}

	delete copyQueue; copyQueue = nullptr;
	delete computeQueue; computeQueue = nullptr;
	delete graphicsQueue; graphicsQueue = nullptr;

	delete RTVStagingDescriptorHeap; RTVStagingDescriptorHeap = nullptr;
	delete DSVStagingDescriptorHeap; DSVStagingDescriptorHeap = nullptr;
	delete SRVStagingDescriptorHeap; SRVStagingDescriptorHeap = nullptr;
	delete SamplerRenderPassDescriptorHeap; SamplerRenderPassDescriptorHeap = nullptr;

	for (uint32_t frameIndex = 0; frameIndex < NUM_FRAMES_IN_FLIGHT; frameIndex++)
	{
		delete SRVRenderPassDescriptorHeaps[frameIndex];
		SRVRenderPassDescriptorHeaps[frameIndex] = nullptr;
		delete uploadContexts[frameIndex];
		uploadContexts[frameIndex] = nullptr;
	}

	SafeRelease(allocator);
	SafeRelease(device);
	SafeRelease(DXGIFactory);

#if defined(_DEBUG)
	IDXGIDebug1* pDebug = nullptr;
	if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&pDebug))))
	{
		pDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_FLAGS(DXGI_DEBUG_RLO_SUMMARY | DXGI_DEBUG_RLO_DETAIL | DXGI_DEBUG_RLO_IGNORE_INTERNAL));
		SafeRelease(pDebug);
	}
#endif
}
//=============================================================================
//=============================================================================
std::unique_ptr<BufferResource> CreateBuffer(const BufferCreationDesc& desc)
{
	std::unique_ptr<BufferResource> newBuffer = std::make_unique<BufferResource>();
	newBuffer->desc.Width = AlignU32(static_cast<uint32_t>(desc.size), 256);
	newBuffer->desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	newBuffer->desc.Alignment = 0;
	newBuffer->desc.Height = 1;
	newBuffer->desc.DepthOrArraySize = 1;
	newBuffer->desc.MipLevels = 1;
	newBuffer->desc.Format = DXGI_FORMAT_UNKNOWN;
	newBuffer->desc.SampleDesc.Count = 1;
	newBuffer->desc.SampleDesc.Quality = 0;
	newBuffer->desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	newBuffer->desc.Flags = D3D12_RESOURCE_FLAG_NONE;
	newBuffer->stride = desc.stride;

	uint32_t numElements = static_cast<uint32_t>(newBuffer->stride > 0 ? desc.size / newBuffer->stride : 1);
	bool isHostVisible = ((desc.accessFlags & BufferAccessFlags::hostWritable) == BufferAccessFlags::hostWritable);
	bool hasCBV = ((desc.viewFlags & BufferViewFlags::cbv) == BufferViewFlags::cbv);
	bool hasSRV = ((desc.viewFlags & BufferViewFlags::srv) == BufferViewFlags::srv);
	bool hasUAV = ((desc.viewFlags & BufferViewFlags::uav) == BufferViewFlags::uav);

	D3D12_RESOURCE_STATES resourceState = D3D12_RESOURCE_STATE_COPY_DEST;

	if (isHostVisible)
	{
		resourceState = D3D12_RESOURCE_STATE_GENERIC_READ;
	}

	newBuffer->state = resourceState;

	D3D12MA::ALLOCATION_DESC allocationDesc{};
	allocationDesc.HeapType = isHostVisible ? D3D12_HEAP_TYPE_UPLOAD : D3D12_HEAP_TYPE_DEFAULT;

	gRHI.allocator->CreateResource(&allocationDesc, &newBuffer->desc, resourceState, nullptr, &newBuffer->allocation, IID_PPV_ARGS(&newBuffer->resource));
	newBuffer->virtualAddress = newBuffer->resource->GetGPUVirtualAddress();

	if (hasCBV)
	{
		D3D12_CONSTANT_BUFFER_VIEW_DESC constantBufferViewDesc = {};
		constantBufferViewDesc.BufferLocation = newBuffer->resource->GetGPUVirtualAddress();
		constantBufferViewDesc.SizeInBytes = static_cast<uint32_t>(newBuffer->desc.Width);

		newBuffer->CBVDescriptor = gRHI.SRVStagingDescriptorHeap->GetNewDescriptor();
		gRHI.device->CreateConstantBufferView(&constantBufferViewDesc, newBuffer->CBVDescriptor.CPUHandle);
	}

	if (hasSRV)
	{
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = desc.isRawAccess ? DXGI_FORMAT_R32_TYPELESS : DXGI_FORMAT_UNKNOWN;
		srvDesc.Buffer.FirstElement = 0;
		srvDesc.Buffer.NumElements = static_cast<uint32_t>(desc.isRawAccess ? (desc.size / 4) : numElements);
		srvDesc.Buffer.StructureByteStride = desc.isRawAccess ? 0 : newBuffer->stride;
		srvDesc.Buffer.Flags = desc.isRawAccess ? D3D12_BUFFER_SRV_FLAG_RAW : D3D12_BUFFER_SRV_FLAG_NONE;

		newBuffer->SRVDescriptor = gRHI.SRVStagingDescriptorHeap->GetNewDescriptor();
		gRHI.device->CreateShaderResourceView(newBuffer->resource, &srvDesc, newBuffer->SRVDescriptor.CPUHandle);

		newBuffer->descriptorHeapIndex = gRHI.FreeReservedDescriptorIndices.back();
		gRHI.FreeReservedDescriptorIndices.pop_back();

		CopySRVHandleToReservedTable(newBuffer->SRVDescriptor, newBuffer->descriptorHeapIndex);
	}

	if (hasUAV)
	{
		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
		uavDesc.Format = desc.isRawAccess ? DXGI_FORMAT_R32_TYPELESS : DXGI_FORMAT_UNKNOWN;
		uavDesc.Buffer.CounterOffsetInBytes = 0;
		uavDesc.Buffer.FirstElement = 0;
		uavDesc.Buffer.NumElements = static_cast<uint32_t>(desc.isRawAccess ? (desc.size / 4) : numElements);
		uavDesc.Buffer.StructureByteStride = desc.isRawAccess ? 0 : newBuffer->stride;
		uavDesc.Buffer.Flags = desc.isRawAccess ? D3D12_BUFFER_UAV_FLAG_RAW : D3D12_BUFFER_UAV_FLAG_NONE;

		newBuffer->UAVDescriptor = gRHI.SRVStagingDescriptorHeap->GetNewDescriptor();
		gRHI.device->CreateUnorderedAccessView(newBuffer->resource, nullptr, &uavDesc, newBuffer->UAVDescriptor.CPUHandle);
	}

	if (isHostVisible)
	{
		newBuffer->resource->Map(0, nullptr, reinterpret_cast<void**>(&newBuffer->mappedResource));
	}

	return newBuffer;
}
//=============================================================================
std::unique_ptr<TextureResource> CreateTexture(const TextureCreationDesc& desc)
{
	D3D12_RESOURCE_DESC textureDesc = desc.resourceDesc;
	textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	bool hasRTV = ((desc.viewFlags & TextureViewFlags::rtv) == TextureViewFlags::rtv);
	bool hasDSV = ((desc.viewFlags & TextureViewFlags::dsv) == TextureViewFlags::dsv);
	bool hasSRV = ((desc.viewFlags & TextureViewFlags::srv) == TextureViewFlags::srv);
	bool hasUAV = ((desc.viewFlags & TextureViewFlags::uav) == TextureViewFlags::uav);

	D3D12_RESOURCE_STATES resourceState = D3D12_RESOURCE_STATE_COPY_DEST;
	DXGI_FORMAT resourceFormat = textureDesc.Format;
	DXGI_FORMAT shaderResourceViewFormat = textureDesc.Format;

	if (hasRTV)
	{
		textureDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
		resourceState = D3D12_RESOURCE_STATE_RENDER_TARGET;
	}

	if (hasDSV)
	{
		switch (desc.resourceDesc.Format)
		{
		case DXGI_FORMAT_D16_UNORM:
			resourceFormat = DXGI_FORMAT_R16_TYPELESS;
			shaderResourceViewFormat = DXGI_FORMAT_R16_UNORM;
			break;
		case DXGI_FORMAT_D24_UNORM_S8_UINT:
			resourceFormat = DXGI_FORMAT_R24G8_TYPELESS;
			shaderResourceViewFormat = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
			break;
		case DXGI_FORMAT_D32_FLOAT:
			resourceFormat = DXGI_FORMAT_R32_TYPELESS;
			shaderResourceViewFormat = DXGI_FORMAT_R32_FLOAT;
			break;
		case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
			resourceFormat = DXGI_FORMAT_R32G8X24_TYPELESS;
			shaderResourceViewFormat = DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
			break;
		default:
			Fatal("Bad depth stencil format.");
			break;
		}

		textureDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
		resourceState = D3D12_RESOURCE_STATE_DEPTH_WRITE;
	}

	if (hasUAV)
	{
		textureDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
		resourceState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	}

	textureDesc.Format = resourceFormat;

	std::unique_ptr<TextureResource> newTexture = std::make_unique<TextureResource>();
	newTexture->desc = textureDesc;
	newTexture->state = resourceState;

	D3D12_CLEAR_VALUE clearValue = {};
	clearValue.Format = desc.resourceDesc.Format;

	if (hasDSV)
	{
		clearValue.DepthStencil.Depth = 1.0f;
	}

	D3D12MA::ALLOCATION_DESC allocationDesc{};
	allocationDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;

	gRHI.allocator->CreateResource(&allocationDesc, &textureDesc, resourceState, (!hasRTV && !hasDSV) ? nullptr : &clearValue, &newTexture->allocation, IID_PPV_ARGS(&newTexture->resource));

	if (hasSRV)
	{
		newTexture->SRVDescriptor = gRHI.SRVStagingDescriptorHeap->GetNewDescriptor();

		if (hasDSV)
		{
			D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
			srvDesc.Format = shaderResourceViewFormat;
			srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srvDesc.Texture2D.MipLevels = 1;
			srvDesc.Texture2D.MostDetailedMip = 0;
			srvDesc.Texture2D.PlaneSlice = 0;
			srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;

			gRHI.device->CreateShaderResourceView(newTexture->resource, &srvDesc, newTexture->SRVDescriptor.CPUHandle);
		}
		else
		{
			D3D12_SHADER_RESOURCE_VIEW_DESC* srvDescPointer = nullptr;
			D3D12_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDesc = {};
			bool isCubeMap = desc.resourceDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D && desc.resourceDesc.DepthOrArraySize == 6;

			if (isCubeMap)
			{
				shaderResourceViewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
				shaderResourceViewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
				shaderResourceViewDesc.TextureCube.MostDetailedMip = 0;
				shaderResourceViewDesc.TextureCube.MipLevels = desc.resourceDesc.MipLevels;
				shaderResourceViewDesc.TextureCube.ResourceMinLODClamp = 0.0f;
				srvDescPointer = &shaderResourceViewDesc;
			}

			gRHI.device->CreateShaderResourceView(newTexture->resource, srvDescPointer, newTexture->SRVDescriptor.CPUHandle);
		}

		newTexture->descriptorHeapIndex = gRHI.FreeReservedDescriptorIndices.back();
		gRHI.FreeReservedDescriptorIndices.pop_back();

		CopySRVHandleToReservedTable(newTexture->SRVDescriptor, newTexture->descriptorHeapIndex);
	}

	if (hasRTV)
	{
		newTexture->RTVDescriptor = gRHI.RTVStagingDescriptorHeap->GetNewDescriptor();
		gRHI.device->CreateRenderTargetView(newTexture->resource, nullptr, newTexture->RTVDescriptor.CPUHandle);
	}

	if (hasDSV)
	{
		D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
		dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
		dsvDesc.Format = desc.resourceDesc.Format;
		dsvDesc.Texture2D.MipSlice = 0;
		dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;

		newTexture->DSVDescriptor = gRHI.DSVStagingDescriptorHeap->GetNewDescriptor();
		gRHI.device->CreateDepthStencilView(newTexture->resource, &dsvDesc, newTexture->DSVDescriptor.CPUHandle);
	}

	if (hasUAV)
	{
		newTexture->UAVDescriptor = gRHI.SRVStagingDescriptorHeap->GetNewDescriptor();
		gRHI.device->CreateUnorderedAccessView(newTexture->resource, nullptr, nullptr, newTexture->UAVDescriptor.CPUHandle);
	}

	newTexture->isReady = (hasRTV || hasDSV);

	return newTexture;
}
//=============================================================================
std::unique_ptr<TextureResource> CreateTextureFromFile(const std::string& texturePath)
{
	auto s2ws = [](const std::string& s)
		{
			//yoink https://stackoverflow.com/questions/27220/how-to-convert-stdstring-to-lpcwstr-in-c-unicode
			int32_t len = 0;
			int32_t slength = (int32_t)s.length() + 1;
			len = MultiByteToWideChar(CP_ACP, 0, s.c_str(), slength, 0, 0);
			wchar_t* buf = new wchar_t[len];
			MultiByteToWideChar(CP_ACP, 0, s.c_str(), slength, buf, len);
			std::wstring r(buf);
			delete[] buf;
			return r;
		};

	std::unique_ptr<DirectX::ScratchImage> imageData = std::make_unique<DirectX::ScratchImage>();
	HRESULT loadResult = DirectX::LoadFromDDSFile(s2ws(texturePath).c_str(), DirectX::DDS_FLAGS_NONE, nullptr, *imageData);
	assert(loadResult == S_OK);

	const DirectX::TexMetadata& textureMetaData = imageData->GetMetadata();
	DXGI_FORMAT textureFormat = textureMetaData.format;
	bool is3DTexture = textureMetaData.dimension == DirectX::TEX_DIMENSION_TEXTURE3D;

	TextureCreationDesc desc;
	desc.resourceDesc.Format = textureFormat;
	desc.resourceDesc.Width = textureMetaData.width;
	desc.resourceDesc.Height = static_cast<UINT>(textureMetaData.height);
	desc.resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
	desc.resourceDesc.DepthOrArraySize = static_cast<UINT16>(is3DTexture ? textureMetaData.depth : textureMetaData.arraySize);
	desc.resourceDesc.MipLevels = static_cast<UINT16>(textureMetaData.mipLevels);
	desc.resourceDesc.SampleDesc.Count = 1;
	desc.resourceDesc.SampleDesc.Quality = 0;
	desc.resourceDesc.Dimension = is3DTexture ? D3D12_RESOURCE_DIMENSION_TEXTURE3D : D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	desc.resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	desc.resourceDesc.Alignment = 0;
	desc.viewFlags = TextureViewFlags::srv;

	auto newTexture = CreateTexture(desc);
	auto textureUpload = std::make_unique<TextureUpload>();

	UINT numRows[MAX_TEXTURE_SUBRESOURCE_COUNT];
	uint64_t rowSizesInBytes[MAX_TEXTURE_SUBRESOURCE_COUNT];

	textureUpload->texture = newTexture.get();
	textureUpload->numSubResources = static_cast<uint32_t>(textureMetaData.mipLevels * textureMetaData.arraySize);

	gRHI.device->GetCopyableFootprints(&desc.resourceDesc, 0, textureUpload->numSubResources, 0, textureUpload->subResourceLayouts.data(), numRows, rowSizesInBytes, &textureUpload->textureDataSize);

	textureUpload->textureData = std::make_unique<uint8_t[]>(textureUpload->textureDataSize);

	for (uint64_t arrayIndex = 0; arrayIndex < textureMetaData.arraySize; arrayIndex++)
	{
		for (uint64_t mipIndex = 0; mipIndex < textureMetaData.mipLevels; mipIndex++)
		{
			const uint64_t subResourceIndex = mipIndex + (arrayIndex * textureMetaData.mipLevels);

			const D3D12_PLACED_SUBRESOURCE_FOOTPRINT& subResourceLayout = textureUpload->subResourceLayouts[subResourceIndex];
			const uint64_t subResourceHeight = numRows[subResourceIndex];
			const uint64_t subResourcePitch = AlignU32(subResourceLayout.Footprint.RowPitch, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);
			const uint64_t subResourceDepth = subResourceLayout.Footprint.Depth;
			uint8_t* destinationSubResourceMemory = textureUpload->textureData.get() + subResourceLayout.Offset;

			for (uint64_t sliceIndex = 0; sliceIndex < subResourceDepth; sliceIndex++)
			{
				const DirectX::Image* subImage = imageData->GetImage(mipIndex, arrayIndex, sliceIndex);
				const uint8_t* sourceSubResourceMemory = subImage->pixels;

				for (uint64_t height = 0; height < subResourceHeight; height++)
				{
					memcpy(destinationSubResourceMemory, sourceSubResourceMemory, (std::min)(subResourcePitch, subImage->rowPitch));
					destinationSubResourceMemory += subResourcePitch;
					sourceSubResourceMemory += subImage->rowPitch;
				}
			}
		}
	}

	gRHI.uploadContexts[gRHI.frameId]->AddTextureUpload(std::move(textureUpload));

	return newTexture;
}
//=============================================================================
std::unique_ptr<Shader> CreateShader(const ShaderCreationDesc& desc)
{
	IDxcUtils* dxcUtils = nullptr;
	IDxcCompiler3* dxcCompiler = nullptr;
	IDxcIncludeHandler* dxcIncludeHandler = nullptr;

	DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&dxcUtils));
	DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxcCompiler));
	dxcUtils->CreateDefaultIncludeHandler(&dxcIncludeHandler);

	std::wstring sourcePath;
	sourcePath.append(SHADER_SOURCE_PATH);
	sourcePath.append(desc.shaderName);

	IDxcBlobEncoding* sourceBlobEncoding = nullptr;
	dxcUtils->LoadFile(sourcePath.c_str(), nullptr, &sourceBlobEncoding);

	DxcBuffer sourceBuffer{};
	sourceBuffer.Ptr = sourceBlobEncoding->GetBufferPointer();
	sourceBuffer.Size = sourceBlobEncoding->GetBufferSize();
	sourceBuffer.Encoding = DXC_CP_ACP;

	LPCWSTR target = nullptr;

	switch (desc.type)
	{
	case ShaderType::vertex:
		target = L"vs_6_6";
		break;
	case ShaderType::pixel:
		target = L"ps_6_6";
		break;
	case ShaderType::compute:
		target = L"cs_6_6";
		break;
	default:
		Fatal("Unimplemented shader type.");
		break;
	}

	std::vector<LPCWSTR> arguments;
	arguments.reserve(8);

	arguments.push_back(desc.shaderName.c_str());
	arguments.push_back(L"-E");
	arguments.push_back(desc.entryPoint.c_str());
	arguments.push_back(L"-T");
	arguments.push_back(target);
	arguments.push_back(L"-Zi");
	arguments.push_back(L"-WX");
	arguments.push_back(L"-Qstrip_reflect");

	IDxcResult* compilationResults = nullptr;
	dxcCompiler->Compile(&sourceBuffer, arguments.data(), static_cast<uint32_t>(arguments.size()), dxcIncludeHandler, IID_PPV_ARGS(&compilationResults));

	IDxcBlobUtf8* errors = nullptr;
	HRESULT getCompilationResults = compilationResults->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errors), nullptr);

	if (FAILED(getCompilationResults))
	{
		Fatal("Failed to get compilation result.");
	}

	if (errors != nullptr && errors->GetStringLength() != 0)
	{
		wprintf(L"Shader compilation error:\n%S\n", errors->GetStringPointer());
		Fatal("Shader compilation error");
	}

	HRESULT statusResult;
	compilationResults->GetStatus(&statusResult);
	if (FAILED(statusResult))
	{
		Fatal("Shader compilation failed");
	}

	std::wstring dxilPath;
	std::wstring pdbPath;

	dxilPath.append(SHADER_OUTPUT_PATH);
	dxilPath.append(desc.shaderName);
	dxilPath.erase(dxilPath.end() - 5, dxilPath.end());
	dxilPath.append(L".dxil");

	pdbPath = dxilPath;
	pdbPath.append(L".pdb");

	IDxcBlob* shaderBlob = nullptr;
	compilationResults->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBlob), nullptr);
	if (shaderBlob != nullptr)
	{
		FILE* fp = nullptr;

		_wfopen_s(&fp, dxilPath.c_str(), L"wb");
		assert(fp);

		fwrite(shaderBlob->GetBufferPointer(), shaderBlob->GetBufferSize(), 1, fp);
		fclose(fp);
	}

	IDxcBlob* pdbBlob = nullptr;
	compilationResults->GetOutput(DXC_OUT_PDB, IID_PPV_ARGS(&pdbBlob), nullptr);
	{
		FILE* fp = nullptr;

		_wfopen_s(&fp, pdbPath.c_str(), L"wb");

		assert(fp);

		fwrite(pdbBlob->GetBufferPointer(), pdbBlob->GetBufferSize(), 1, fp);
		fclose(fp);
	}

	SafeRelease(pdbBlob);
	SafeRelease(errors);
	SafeRelease(compilationResults);
	SafeRelease(dxcIncludeHandler);
	SafeRelease(dxcCompiler);
	SafeRelease(dxcUtils);

	std::unique_ptr<Shader> shader = std::make_unique<Shader>();
	shader->shaderBlob = shaderBlob;

	return shader;
}
//=============================================================================
ID3D12RootSignature* CreateRootSignature(const PipelineResourceLayout& layout, PipelineResourceMapping& resourceMapping)
{
	std::vector<D3D12_ROOT_PARAMETER1> rootParameters;
	std::array<std::vector<D3D12_DESCRIPTOR_RANGE1>, NUM_RESOURCE_SPACES> desciptorRanges;

	for (uint32_t spaceId = 0; spaceId < NUM_RESOURCE_SPACES; spaceId++)
	{
		PipelineResourceSpace* currentSpace = layout.spaces[spaceId];
		std::vector<D3D12_DESCRIPTOR_RANGE1>& currentDescriptorRange = desciptorRanges[spaceId];

		if (currentSpace)
		{
			const BufferResource* cbv = currentSpace->GetCBV();
			auto& uavs = currentSpace->GetUAVs();
			auto& srvs = currentSpace->GetSRVs();

			if (cbv)
			{
				D3D12_ROOT_PARAMETER1 rootParameter{};
				rootParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
				rootParameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
				rootParameter.Descriptor.RegisterSpace = spaceId;
				rootParameter.Descriptor.ShaderRegister = 0;

				resourceMapping.cbvMapping[spaceId] = static_cast<uint32_t>(rootParameters.size());
				rootParameters.push_back(rootParameter);
			}

			if (uavs.empty() && srvs.empty())
			{
				continue;
			}

			for (auto& uav : uavs)
			{
				D3D12_DESCRIPTOR_RANGE1 range{};
				range.BaseShaderRegister = uav.bindingIndex;
				range.NumDescriptors = 1;
				range.OffsetInDescriptorsFromTableStart = static_cast<uint32_t>(currentDescriptorRange.size());
				range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
				range.RegisterSpace = spaceId;
				range.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE | D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE;

				currentDescriptorRange.push_back(range);
			}

			for (auto& srv : srvs)
			{
				D3D12_DESCRIPTOR_RANGE1 range{};
				range.BaseShaderRegister = srv.bindingIndex;
				range.NumDescriptors = 1;
				range.OffsetInDescriptorsFromTableStart = static_cast<uint32_t>(currentDescriptorRange.size());
				range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
				range.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE | D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE;
				range.RegisterSpace = spaceId;

				currentDescriptorRange.push_back(range);
			}

			D3D12_ROOT_PARAMETER1 desciptorTableForSpace{};
			desciptorTableForSpace.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			desciptorTableForSpace.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
			desciptorTableForSpace.DescriptorTable.NumDescriptorRanges = static_cast<uint32_t>(currentDescriptorRange.size());
			desciptorTableForSpace.DescriptorTable.pDescriptorRanges = currentDescriptorRange.data();

			resourceMapping.tableMapping[spaceId] = static_cast<uint32_t>(rootParameters.size());
			rootParameters.push_back(desciptorTableForSpace);
		}
	}

	D3D12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc{};
	rootSignatureDesc.Desc_1_1.NumParameters = static_cast<uint32_t>(rootParameters.size());
	rootSignatureDesc.Desc_1_1.pParameters = rootParameters.data();
	rootSignatureDesc.Desc_1_1.NumStaticSamplers = 0;
	rootSignatureDesc.Desc_1_1.pStaticSamplers = nullptr;
	rootSignatureDesc.Desc_1_1.Flags = D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED | D3D12_ROOT_SIGNATURE_FLAG_SAMPLER_HEAP_DIRECTLY_INDEXED;
	rootSignatureDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;

	ID3DBlob* rootSignatureBlob = nullptr;
	ID3DBlob* errorBlob = nullptr;
	HRESULT result = D3D12SerializeVersionedRootSignature(&rootSignatureDesc, &rootSignatureBlob, &errorBlob);
	if (FAILED(result))
	{
		Fatal("D3D12SerializeVersionedRootSignature() failed: " + DXErrorToStr(result));
		return nullptr;
	}

	ID3D12RootSignature* rootSignature = nullptr;
	result = gRHI.device->CreateRootSignature(0, rootSignatureBlob->GetBufferPointer(), rootSignatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature));
	if (FAILED(result))
	{
		Fatal("ID3D12Device8::CreateRootSignature() failed: " + DXErrorToStr(result));
		return nullptr;
	}

	return rootSignature;
}
//=============================================================================
std::unique_ptr<PipelineStateObject> CreateGraphicsPipeline(const GraphicsPipelineDesc& desc, const PipelineResourceLayout& layout)
{
	D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineDesc{};
	pipelineDesc.NodeMask = 0;
	pipelineDesc.SampleMask = 0xFFFFFFFF;
	pipelineDesc.PrimitiveTopologyType = desc.topology;
	pipelineDesc.InputLayout.pInputElementDescs = nullptr;
	pipelineDesc.InputLayout.NumElements = 0;
	pipelineDesc.RasterizerState = desc.rasterDesc;
	pipelineDesc.BlendState = desc.blendDesc;
	pipelineDesc.SampleDesc = desc.sampleDesc;
	pipelineDesc.DepthStencilState = desc.depthStencilDesc;
	pipelineDesc.DSVFormat = desc.renderTargetDesc.depthStencilFormat;

	pipelineDesc.NumRenderTargets = desc.renderTargetDesc.numRenderTargets;
	for (uint32_t rtvIndex = 0; rtvIndex < pipelineDesc.NumRenderTargets; rtvIndex++)
	{
		pipelineDesc.RTVFormats[rtvIndex] = desc.renderTargetDesc.renderTargetFormats[rtvIndex];
	}

	if (desc.vertexShader)
	{
		pipelineDesc.VS.pShaderBytecode = desc.vertexShader->shaderBlob->GetBufferPointer();
		pipelineDesc.VS.BytecodeLength = desc.vertexShader->shaderBlob->GetBufferSize();
	}

	if (desc.pixelShader)
	{
		pipelineDesc.PS.pShaderBytecode = desc.pixelShader->shaderBlob->GetBufferPointer();
		pipelineDesc.PS.BytecodeLength = desc.pixelShader->shaderBlob->GetBufferSize();
	}

	std::unique_ptr<PipelineStateObject> newPipeline = std::make_unique<PipelineStateObject>();
	newPipeline->pipelineType = PipelineType::graphics;

	pipelineDesc.pRootSignature = CreateRootSignature(layout, newPipeline->pipelineResourceMapping);

	ID3D12PipelineState* graphicsPipeline = nullptr;
	HRESULT result = gRHI.device->CreateGraphicsPipelineState(&pipelineDesc, IID_PPV_ARGS(&graphicsPipeline));
	if (FAILED(result))
	{
		Fatal("ID3D12Device8::CreateGraphicsPipelineState() failed: " + DXErrorToStr(result));
		return nullptr;
	}

	newPipeline->pipeline = graphicsPipeline;
	newPipeline->rootSignature = pipelineDesc.pRootSignature;

	return newPipeline;
}
//=============================================================================
std::unique_ptr<PipelineStateObject> CreateComputePipeline(const ComputePipelineDesc& desc, const PipelineResourceLayout& layout)
{
	std::unique_ptr<PipelineStateObject> newPipeline = std::make_unique<PipelineStateObject>();
	newPipeline->pipelineType = PipelineType::compute;

	D3D12_COMPUTE_PIPELINE_STATE_DESC pipelineDesc{};
	pipelineDesc.NodeMask = 0;
	pipelineDesc.CS.pShaderBytecode = desc.computeShader->shaderBlob->GetBufferPointer();
	pipelineDesc.CS.BytecodeLength = desc.computeShader->shaderBlob->GetBufferSize();
	pipelineDesc.pRootSignature = CreateRootSignature(layout, newPipeline->pipelineResourceMapping);

	ID3D12PipelineState* computePipeline = nullptr;
	HRESULT result = gRHI.device->CreateComputePipelineState(&pipelineDesc, IID_PPV_ARGS(&computePipeline));
	if (FAILED(result))
	{
		Fatal("ID3D12Device8::CreateComputePipelineState() failed: " + DXErrorToStr(result));
		return nullptr;
	}

	newPipeline->pipeline = computePipeline;
	newPipeline->rootSignature = pipelineDesc.pRootSignature;

	return newPipeline;
}
//=============================================================================
std::unique_ptr<GraphicsContext> CreateGraphicsContext()
{
	std::unique_ptr<GraphicsContext> newGraphicsContext = std::make_unique<GraphicsContext>();
	return newGraphicsContext;
}
//=============================================================================
std::unique_ptr<ComputeContext> CreateComputeContext()
{
	std::unique_ptr<ComputeContext> newComputeContext = std::make_unique<ComputeContext>();
	return newComputeContext;
}
//=============================================================================
void DestroyBuffer(std::unique_ptr<BufferResource> buffer)
{
	gRHI.destructionQueues[gRHI.frameId].buffersToDestroy.push_back(std::move(buffer));
}
//=============================================================================
void DestroyTexture(std::unique_ptr<TextureResource> texture)
{
	gRHI.destructionQueues[gRHI.frameId].texturesToDestroy.push_back(std::move(texture));
}
//=============================================================================
void DestroyShader(std::unique_ptr<Shader> shader)
{
	SafeRelease(shader->shaderBlob);
}
//=============================================================================
void DestroyPipelineStateObject(std::unique_ptr<PipelineStateObject> pso)
{
	gRHI.destructionQueues[gRHI.frameId].pipelinesToDestroy.push_back(std::move(pso));
}
//=============================================================================
void DestroyContext(std::unique_ptr<Context> context)
{
	gRHI.destructionQueues[gRHI.frameId].contextsToDestroy.push_back(std::move(context));
}
//=============================================================================
ContextSubmissionResult SubmitContextWork(Context& context)
{
	uint64_t fenceResult = 0;

	switch (context.GetCommandType())
	{
	case D3D12_COMMAND_LIST_TYPE_DIRECT:
		fenceResult = gRHI.graphicsQueue->ExecuteCommandList(context.GetCommandList());
		break;
	case D3D12_COMMAND_LIST_TYPE_COMPUTE:
		fenceResult = gRHI.computeQueue->ExecuteCommandList(context.GetCommandList());
		break;
	case D3D12_COMMAND_LIST_TYPE_COPY:
		fenceResult = gRHI.copyQueue->ExecuteCommandList(context.GetCommandList());
		break;
	default:
		Fatal("Unsupported submission type.");
		break;
	}

	ContextSubmissionResult submissionResult;
	submissionResult.frameId = gRHI.frameId;
	submissionResult.submissionIndex = static_cast<uint32_t>(gRHI.contextSubmissions[gRHI.frameId].size());

	gRHI.contextSubmissions[gRHI.frameId].push_back(std::make_pair(fenceResult, context.GetCommandType()));

	return submissionResult;
}
//=============================================================================
void WaitOnContextWork(ContextSubmissionResult submission, ContextWaitType waitType)
{
	std::pair<uint64_t, D3D12_COMMAND_LIST_TYPE> contextSubmission = gRHI.contextSubmissions[submission.frameId][submission.submissionIndex];
	QueueD3D12* workSourceQueue = nullptr;

	switch (contextSubmission.second)
	{
	case D3D12_COMMAND_LIST_TYPE_DIRECT:
		workSourceQueue = gRHI.graphicsQueue;
		break;
	case D3D12_COMMAND_LIST_TYPE_COMPUTE:
		workSourceQueue = gRHI.computeQueue;
		break;
	case D3D12_COMMAND_LIST_TYPE_COPY:
		workSourceQueue = gRHI.copyQueue;
		break;
	default:
		Fatal("Unsupported submission type.");
		break;
	}

	switch (waitType)
	{
	case ContextWaitType::graphics:
		gRHI.graphicsQueue->InsertWaitForQueueFence(workSourceQueue, contextSubmission.first);
		break;
	case ContextWaitType::compute:
		gRHI.computeQueue->InsertWaitForQueueFence(workSourceQueue, contextSubmission.first);
		break;
	case ContextWaitType::copy:
		gRHI.copyQueue->InsertWaitForQueueFence(workSourceQueue, contextSubmission.first);
		break;
	case ContextWaitType::host:
		workSourceQueue->WaitForFenceCPUBlocking(contextSubmission.first);
		break;
	default:
		Fatal("Unsupported wait type.");
		break;
	}
}
//=============================================================================
void WaitForIdle()
{
	if (gRHI.graphicsQueue) gRHI.graphicsQueue->WaitForIdle();
	if (gRHI.computeQueue) gRHI.computeQueue->WaitForIdle();
	if (gRHI.copyQueue) gRHI.copyQueue->WaitForIdle();
}
//=============================================================================
void CopyDescriptorsSimple(uint32_t numDescriptors, D3D12_CPU_DESCRIPTOR_HANDLE destDescriptorRangeStart, D3D12_CPU_DESCRIPTOR_HANDLE srcDescriptorRangeStart, D3D12_DESCRIPTOR_HEAP_TYPE descriptorType)
{
	gRHI.device->CopyDescriptorsSimple(numDescriptors, destDescriptorRangeStart, srcDescriptorRangeStart, descriptorType);
}
//=============================================================================
void CopyDescriptors(uint32_t numDestDescriptorRanges, const D3D12_CPU_DESCRIPTOR_HANDLE* destDescriptorRangeStarts, const uint32_t* destDescriptorRangeSizes,
	uint32_t numSrcDescriptorRanges, const D3D12_CPU_DESCRIPTOR_HANDLE* srcDescriptorRangeStarts, const uint32_t* srcDescriptorRangeSizes, D3D12_DESCRIPTOR_HEAP_TYPE descriptorType)
{
	gRHI.device->CopyDescriptors(numDestDescriptorRanges, destDescriptorRangeStarts, destDescriptorRangeSizes, numSrcDescriptorRanges, srcDescriptorRangeStarts, srcDescriptorRangeSizes, descriptorType);
}
//=============================================================================
void ProcessDestructions(uint32_t frameIndex)
{
	auto& destructionQueueForFrame = gRHI.destructionQueues[frameIndex];

	for (auto& bufferToDestroy : destructionQueueForFrame.buffersToDestroy)
	{
		if (bufferToDestroy->CBVDescriptor.IsValid())
		{
			gRHI.SRVStagingDescriptorHeap->FreeDescriptor(bufferToDestroy->CBVDescriptor);
		}

		if (bufferToDestroy->SRVDescriptor.IsValid())
		{
			gRHI.SRVStagingDescriptorHeap->FreeDescriptor(bufferToDestroy->SRVDescriptor);
			gRHI.FreeReservedDescriptorIndices.push_back(bufferToDestroy->descriptorHeapIndex);
		}

		if (bufferToDestroy->UAVDescriptor.IsValid())
		{
			gRHI.SRVStagingDescriptorHeap->FreeDescriptor(bufferToDestroy->UAVDescriptor);
		}

		if (bufferToDestroy->mappedResource != nullptr)
		{
			bufferToDestroy->resource->Unmap(0, nullptr);
		}

		SafeRelease(bufferToDestroy->resource);
		SafeRelease(bufferToDestroy->allocation);
	}

	for (auto& textureToDestroy : destructionQueueForFrame.texturesToDestroy)
	{
		if (textureToDestroy->RTVDescriptor.IsValid())
		{
			gRHI.RTVStagingDescriptorHeap->FreeDescriptor(textureToDestroy->RTVDescriptor);
		}

		if (textureToDestroy->DSVDescriptor.IsValid())
		{
			gRHI.DSVStagingDescriptorHeap->FreeDescriptor(textureToDestroy->DSVDescriptor);
		}

		if (textureToDestroy->SRVDescriptor.IsValid())
		{
			gRHI.SRVStagingDescriptorHeap->FreeDescriptor(textureToDestroy->SRVDescriptor);
			gRHI.FreeReservedDescriptorIndices.push_back(textureToDestroy->descriptorHeapIndex);
		}

		if (textureToDestroy->UAVDescriptor.IsValid())
		{
			gRHI.SRVStagingDescriptorHeap->FreeDescriptor(textureToDestroy->UAVDescriptor);
		}

		SafeRelease(textureToDestroy->resource);
		SafeRelease(textureToDestroy->allocation);
	}

	for (auto& pipelineToDestroy : destructionQueueForFrame.pipelinesToDestroy)
	{
		SafeRelease(pipelineToDestroy->rootSignature);
		SafeRelease(pipelineToDestroy->pipeline);
	}

	destructionQueueForFrame.buffersToDestroy.clear();
	destructionQueueForFrame.texturesToDestroy.clear();
	destructionQueueForFrame.pipelinesToDestroy.clear();
	destructionQueueForFrame.contextsToDestroy.clear();
}
//=============================================================================
#endif // RENDER_D3D12