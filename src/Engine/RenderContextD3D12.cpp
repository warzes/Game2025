#include "stdafx.h"
#if RENDER_D3D12
#include "RenderContextD3D12.h"
#include "RenderCore.h"
#include "Log.h"
//=============================================================================
void CopySRVHandleToReservedTable(Descriptor srvHandle, uint32_t index)
{
	for (uint32_t frameIndex = 0; frameIndex < NUM_FRAMES_IN_FLIGHT; frameIndex++)
	{
		Descriptor targetDescriptor = gRenderContext.SRVRenderPassDescriptorHeaps[frameIndex]->GetReservedDescriptor(index);

		CopyDescriptorsSimple(1, targetDescriptor.CPUHandle, srvHandle.CPUHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	}
}
//=============================================================================
RenderContext::~RenderContext()
{
	assert(!device);
}
//=============================================================================
void RenderContext::Release()
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
TextureResource& RenderContext::GetCurrentBackBuffer()
{
	return *backBuffers[swapChain->GetCurrentBackBufferIndex()];
}
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

	gRenderContext.allocator->CreateResource(&allocationDesc, &textureDesc, resourceState, (!hasRTV && !hasDSV) ? nullptr : &clearValue, &newTexture->allocation, IID_PPV_ARGS(&newTexture->resource));

	if (hasSRV)
	{
		newTexture->SRVDescriptor = gRenderContext.SRVStagingDescriptorHeap->GetNewDescriptor();

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

			gRenderContext.device->CreateShaderResourceView(newTexture->resource, &srvDesc, newTexture->SRVDescriptor.CPUHandle);
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

			gRenderContext.device->CreateShaderResourceView(newTexture->resource, srvDescPointer, newTexture->SRVDescriptor.CPUHandle);
		}

		newTexture->descriptorHeapIndex = gRenderContext.FreeReservedDescriptorIndices.back();
		gRenderContext.FreeReservedDescriptorIndices.pop_back();

		CopySRVHandleToReservedTable(newTexture->SRVDescriptor, newTexture->descriptorHeapIndex);
	}

	if (hasRTV)
	{
		newTexture->RTVDescriptor = gRenderContext.RTVStagingDescriptorHeap->GetNewDescriptor();
		gRenderContext.device->CreateRenderTargetView(newTexture->resource, nullptr, newTexture->RTVDescriptor.CPUHandle);
	}

	if (hasDSV)
	{
		D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
		dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
		dsvDesc.Format = desc.resourceDesc.Format;
		dsvDesc.Texture2D.MipSlice = 0;
		dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;

		newTexture->DSVDescriptor = gRenderContext.DSVStagingDescriptorHeap->GetNewDescriptor();
		gRenderContext.device->CreateDepthStencilView(newTexture->resource, &dsvDesc, newTexture->DSVDescriptor.CPUHandle);
	}

	if (hasUAV)
	{
		newTexture->UAVDescriptor = gRenderContext.SRVStagingDescriptorHeap->GetNewDescriptor();
		gRenderContext.device->CreateUnorderedAccessView(newTexture->resource, nullptr, nullptr, newTexture->UAVDescriptor.CPUHandle);
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

	gRenderContext.device->GetCopyableFootprints(&desc.resourceDesc, 0, textureUpload->numSubResources, 0, textureUpload->subResourceLayouts.data(), numRows, rowSizesInBytes, &textureUpload->textureDataSize);

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

	gRenderContext.uploadContexts[gRenderContext.frameId]->AddTextureUpload(std::move(textureUpload));

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
	result = gRenderContext.device->CreateRootSignature(0, rootSignatureBlob->GetBufferPointer(), rootSignatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature));
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
	HRESULT result = gRenderContext.device->CreateGraphicsPipelineState(&pipelineDesc, IID_PPV_ARGS(&graphicsPipeline));
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
	HRESULT result = gRenderContext.device->CreateComputePipelineState(&pipelineDesc, IID_PPV_ARGS(&computePipeline));
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
	gRenderContext.destructionQueues[gRenderContext.frameId].buffersToDestroy.push_back(std::move(buffer));
}
//=============================================================================
void DestroyTexture(std::unique_ptr<TextureResource> texture)
{
	gRenderContext.destructionQueues[gRenderContext.frameId].texturesToDestroy.push_back(std::move(texture));
}
//=============================================================================
void DestroyShader(std::unique_ptr<Shader> shader)
{
	SafeRelease(shader->shaderBlob);
}
//=============================================================================
void DestroyPipelineStateObject(std::unique_ptr<PipelineStateObject> pso)
{
	gRenderContext.destructionQueues[gRenderContext.frameId].pipelinesToDestroy.push_back(std::move(pso));
}
//=============================================================================
void DestroyContext(std::unique_ptr<Context> context)
{
	gRenderContext.destructionQueues[gRenderContext.frameId].contextsToDestroy.push_back(std::move(context));
}
//=============================================================================
ContextSubmissionResult SubmitContextWork(Context& context)
{
	uint64_t fenceResult = 0;

	switch (context.GetCommandType())
	{
	case D3D12_COMMAND_LIST_TYPE_DIRECT:
		fenceResult = gRenderContext.graphicsQueue->ExecuteCommandList(context.GetCommandList());
		break;
	case D3D12_COMMAND_LIST_TYPE_COMPUTE:
		fenceResult = gRenderContext.computeQueue->ExecuteCommandList(context.GetCommandList());
		break;
	case D3D12_COMMAND_LIST_TYPE_COPY:
		fenceResult = gRenderContext.copyQueue->ExecuteCommandList(context.GetCommandList());
		break;
	default:
		Fatal("Unsupported submission type.");
		break;
	}

	ContextSubmissionResult submissionResult;
	submissionResult.frameId = gRenderContext.frameId;
	submissionResult.submissionIndex = static_cast<uint32_t>(gRenderContext.contextSubmissions[gRenderContext.frameId].size());

	gRenderContext.contextSubmissions[gRenderContext.frameId].push_back(std::make_pair(fenceResult, context.GetCommandType()));

	return submissionResult;
}
//=============================================================================
void WaitOnContextWork(ContextSubmissionResult submission, ContextWaitType waitType)
{
	std::pair<uint64_t, D3D12_COMMAND_LIST_TYPE> contextSubmission = gRenderContext.contextSubmissions[submission.frameId][submission.submissionIndex];
	QueueD3D12* workSourceQueue = nullptr;

	switch (contextSubmission.second)
	{
	case D3D12_COMMAND_LIST_TYPE_DIRECT:
		workSourceQueue = gRenderContext.graphicsQueue;
		break;
	case D3D12_COMMAND_LIST_TYPE_COMPUTE:
		workSourceQueue = gRenderContext.computeQueue;
		break;
	case D3D12_COMMAND_LIST_TYPE_COPY:
		workSourceQueue = gRenderContext.copyQueue;
		break;
	default:
		Fatal("Unsupported submission type.");
		break;
	}

	switch (waitType)
	{
	case ContextWaitType::graphics:
		gRenderContext.graphicsQueue->InsertWaitForQueueFence(workSourceQueue, contextSubmission.first);
		break;
	case ContextWaitType::compute:
		gRenderContext.computeQueue->InsertWaitForQueueFence(workSourceQueue, contextSubmission.first);
		break;
	case ContextWaitType::copy:
		gRenderContext.copyQueue->InsertWaitForQueueFence(workSourceQueue, contextSubmission.first);
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
	if (gRenderContext.graphicsQueue) gRenderContext.graphicsQueue->WaitForIdle();
	if (gRenderContext.computeQueue) gRenderContext.computeQueue->WaitForIdle();
	if (gRenderContext.copyQueue) gRenderContext.copyQueue->WaitForIdle();
}
//=============================================================================
void CopyDescriptorsSimple(uint32_t numDescriptors, D3D12_CPU_DESCRIPTOR_HANDLE destDescriptorRangeStart, D3D12_CPU_DESCRIPTOR_HANDLE srcDescriptorRangeStart, D3D12_DESCRIPTOR_HEAP_TYPE descriptorType)
{
	gRenderContext.device->CopyDescriptorsSimple(numDescriptors, destDescriptorRangeStart, srcDescriptorRangeStart, descriptorType);
}
//=============================================================================
void CopyDescriptors(uint32_t numDestDescriptorRanges, const D3D12_CPU_DESCRIPTOR_HANDLE* destDescriptorRangeStarts, const uint32_t* destDescriptorRangeSizes,
	uint32_t numSrcDescriptorRanges, const D3D12_CPU_DESCRIPTOR_HANDLE* srcDescriptorRangeStarts, const uint32_t* srcDescriptorRangeSizes, D3D12_DESCRIPTOR_HEAP_TYPE descriptorType)
{
	gRenderContext.device->CopyDescriptors(numDestDescriptorRanges, destDescriptorRangeStarts, destDescriptorRangeSizes, numSrcDescriptorRanges, srcDescriptorRangeStarts, srcDescriptorRangeSizes, descriptorType);
}
//=============================================================================
void ProcessDestructions(uint32_t frameIndex)
{
	auto& destructionQueueForFrame = gRenderContext.destructionQueues[frameIndex];

	for (auto& bufferToDestroy : destructionQueueForFrame.buffersToDestroy)
	{
		if (bufferToDestroy->CBVDescriptor.IsValid())
		{
			gRenderContext.SRVStagingDescriptorHeap->FreeDescriptor(bufferToDestroy->CBVDescriptor);
		}

		if (bufferToDestroy->SRVDescriptor.IsValid())
		{
			gRenderContext.SRVStagingDescriptorHeap->FreeDescriptor(bufferToDestroy->SRVDescriptor);
			gRenderContext.FreeReservedDescriptorIndices.push_back(bufferToDestroy->descriptorHeapIndex);
		}

		if (bufferToDestroy->UAVDescriptor.IsValid())
		{
			gRenderContext.SRVStagingDescriptorHeap->FreeDescriptor(bufferToDestroy->UAVDescriptor);
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
			gRenderContext.RTVStagingDescriptorHeap->FreeDescriptor(textureToDestroy->RTVDescriptor);
		}

		if (textureToDestroy->DSVDescriptor.IsValid())
		{
			gRenderContext.DSVStagingDescriptorHeap->FreeDescriptor(textureToDestroy->DSVDescriptor);
		}

		if (textureToDestroy->SRVDescriptor.IsValid())
		{
			gRenderContext.SRVStagingDescriptorHeap->FreeDescriptor(textureToDestroy->SRVDescriptor);
			gRenderContext.FreeReservedDescriptorIndices.push_back(textureToDestroy->descriptorHeapIndex);
		}

		if (textureToDestroy->UAVDescriptor.IsValid())
		{
			gRenderContext.SRVStagingDescriptorHeap->FreeDescriptor(textureToDestroy->UAVDescriptor);
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