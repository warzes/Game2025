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
void CopySRVHandleToReservedTable(Descriptor srvHandle, uint32_t index)
{
	for (uint32_t frameIndex = 0; frameIndex < NUM_FRAMES_IN_FLIGHT; frameIndex++)
	{
		Descriptor targetDescriptor = gRenderContext.SRVRenderPassDescriptorHeaps[frameIndex]->GetReservedDescriptor(index);

		RHIBackend::CopyDescriptorsSimple(1, targetDescriptor.CPUHandle, srvHandle.CPUHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	}
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
	result = D3D12CreateDevice(gRenderContext.adapter, D3D_FEATURE_LEVEL_12_2, IID_PPV_ARGS(&gRenderContext.device));
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
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);

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


	return true;
}
//=============================================================================
void RHIBackend::DestroyAPI()
{
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
void RHIBackend::Present()
{
}
//=============================================================================
std::unique_ptr<BufferResource> RHIBackend::CreateBuffer(const BufferCreationDesc& desc)
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

	gRenderContext.allocator->CreateResource(&allocationDesc, &newBuffer->desc, resourceState, nullptr, &newBuffer->allocation, IID_PPV_ARGS(&newBuffer->resource));
	newBuffer->virtualAddress = newBuffer->resource->GetGPUVirtualAddress();

	if (hasCBV)
	{
		D3D12_CONSTANT_BUFFER_VIEW_DESC constantBufferViewDesc = {};
		constantBufferViewDesc.BufferLocation = newBuffer->resource->GetGPUVirtualAddress();
		constantBufferViewDesc.SizeInBytes = static_cast<uint32_t>(newBuffer->desc.Width);

		newBuffer->CBVDescriptor = gRenderContext.SRVStagingDescriptorHeap->GetNewDescriptor();
		gRenderContext.device->CreateConstantBufferView(&constantBufferViewDesc, newBuffer->CBVDescriptor.CPUHandle);
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

		newBuffer->SRVDescriptor = gRenderContext.SRVStagingDescriptorHeap->GetNewDescriptor();
		gRenderContext.device->CreateShaderResourceView(newBuffer->resource, &srvDesc, newBuffer->SRVDescriptor.CPUHandle);

		newBuffer->descriptorHeapIndex = gRenderContext.FreeReservedDescriptorIndices.back();
		gRenderContext.FreeReservedDescriptorIndices.pop_back();

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

		newBuffer->UAVDescriptor = gRenderContext.SRVStagingDescriptorHeap->GetNewDescriptor();
		gRenderContext.device->CreateUnorderedAccessView(newBuffer->resource, nullptr, &uavDesc, newBuffer->UAVDescriptor.CPUHandle);
	}

	if (isHostVisible)
	{
		newBuffer->resource->Map(0, nullptr, reinterpret_cast<void**>(&newBuffer->mappedResource));
	}

	return newBuffer;
}
//=============================================================================
void RHIBackend::CopyDescriptorsSimple(uint32_t numDescriptors, D3D12_CPU_DESCRIPTOR_HANDLE destDescriptorRangeStart, D3D12_CPU_DESCRIPTOR_HANDLE srcDescriptorRangeStart, D3D12_DESCRIPTOR_HEAP_TYPE descriptorType)
{
	gRenderContext.device->CopyDescriptorsSimple(numDescriptors, destDescriptorRangeStart, srcDescriptorRangeStart, descriptorType);
}
//=============================================================================
#endif // RENDER_D3D12