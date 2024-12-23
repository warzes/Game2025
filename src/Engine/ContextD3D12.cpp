﻿#include "stdafx.h"
#if RENDER_D3D12
#include "ContextD3D12.h"
#include "RenderContextD3D12.h"
#include "Log.h"
//=============================================================================
Context::Context(D3D12_COMMAND_LIST_TYPE commandType) : m_contextType(commandType)
{
	HRESULT result;
	for (uint32_t frameIndex = 0; frameIndex < NUM_FRAMES_IN_FLIGHT; frameIndex++)
	{
		result = gRenderContext.device->CreateCommandAllocator(commandType, IID_PPV_ARGS(&m_commandAllocators[frameIndex]));
		if (FAILED(result))
		{
			Fatal("ID3D12Device::CreateCommandAllocator() failed: " + DXErrorToStr(result));
			return;
		}
	}

	result = gRenderContext.device->CreateCommandList1(0, commandType, D3D12_COMMAND_LIST_FLAG_NONE, IID_PPV_ARGS(&m_commandList));
	if (FAILED(result))
	{
		Fatal("ID3D12Device::CreateCommandList1() failed: " + DXErrorToStr(result));
		return;
	}
}
//=============================================================================
Context::~Context()
{
	SafeRelease(m_commandList);

	for (uint32_t frameIndex = 0; frameIndex < NUM_FRAMES_IN_FLIGHT; frameIndex++)
	{
		SafeRelease(m_commandAllocators[frameIndex]);
	}
}
//=============================================================================
void Context::Reset()
{
	uint32_t frameId = gRenderContext.GetFrameId();

	m_commandAllocators[frameId]->Reset();
	m_commandList->Reset(m_commandAllocators[frameId], nullptr);

	if (m_contextType != D3D12_COMMAND_LIST_TYPE_COPY)
	{
		bindDescriptorHeaps(gRenderContext.GetFrameId());
	}
}
//=============================================================================
void Context::AddBarrier(Resource& resource, D3D12_RESOURCE_STATES newState)
{
	if (m_numQueuedBarriers >= MAX_QUEUED_BARRIERS)
	{
		FlushBarriers();
	}

	D3D12_RESOURCE_STATES oldState = resource.state;

	if (m_contextType == D3D12_COMMAND_LIST_TYPE_COMPUTE)
	{
		constexpr D3D12_RESOURCE_STATES VALID_COMPUTE_CONTEXT_STATES = (D3D12_RESOURCE_STATE_UNORDERED_ACCESS | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE |
			D3D12_RESOURCE_STATE_COPY_DEST | D3D12_RESOURCE_STATE_COPY_SOURCE);

		assert((oldState & VALID_COMPUTE_CONTEXT_STATES) == oldState);
		assert((newState & VALID_COMPUTE_CONTEXT_STATES) == newState);
	}

	if (oldState != newState)
	{
		D3D12_RESOURCE_BARRIER& barrierDesc = m_resourceBarriers[m_numQueuedBarriers];
		m_numQueuedBarriers++;

		barrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrierDesc.Transition.pResource = resource.resource;
		barrierDesc.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		barrierDesc.Transition.StateBefore = oldState;
		barrierDesc.Transition.StateAfter = newState;
		barrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;

		resource.state = newState;
	}
	else if (newState == D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
	{
		D3D12_RESOURCE_BARRIER& barrierDesc = m_resourceBarriers[m_numQueuedBarriers];
		m_numQueuedBarriers++;

		barrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
		barrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		barrierDesc.UAV.pResource = resource.resource;
	}
}
//=============================================================================
void Context::FlushBarriers()
{
	if (m_numQueuedBarriers > 0)
	{
		m_commandList->ResourceBarrier(m_numQueuedBarriers, m_resourceBarriers.data());
		m_numQueuedBarriers = 0;
	}
}
//=============================================================================
void Context::bindDescriptorHeaps(uint32_t frameIndex)
{
	m_currentSRVHeap = &gRenderContext.GetSRVHeap(frameIndex);
	m_currentSRVHeap->Reset();

	ID3D12DescriptorHeap* heapsToBind[2];
	heapsToBind[0] = gRenderContext.GetSRVHeap(frameIndex).GetHeap();
	heapsToBind[1] = gRenderContext.GetSamplerHeap().GetHeap();

	m_commandList->SetDescriptorHeaps(2, heapsToBind);
}
//=============================================================================
void Context::CopyResource(const Resource& destination, const Resource& source)
{
	m_commandList->CopyResource(destination.resource, source.resource);
}
//=============================================================================
void Context::CopyBufferRegion(Resource& destination, uint64_t destOffset, Resource& source, uint64_t sourceOffset, uint64_t numBytes)
{
	m_commandList->CopyBufferRegion(destination.resource, destOffset, source.resource, sourceOffset, numBytes);
}
//=============================================================================
void Context::CopyTextureRegion(Resource& destination, Resource& source, size_t sourceOffset, SubResourceLayouts& subResourceLayouts, uint32_t numSubResources)
{
	for (uint32_t subResourceIndex = 0; subResourceIndex < numSubResources; subResourceIndex++)
	{
		D3D12_TEXTURE_COPY_LOCATION destinationLocation = {};
		destinationLocation.pResource = destination.resource;
		destinationLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		destinationLocation.SubresourceIndex = subResourceIndex;

		D3D12_TEXTURE_COPY_LOCATION sourceLocation = {};
		sourceLocation.pResource = source.resource;
		sourceLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
		sourceLocation.PlacedFootprint = subResourceLayouts[subResourceIndex];
		sourceLocation.PlacedFootprint.Offset += sourceOffset;

		m_commandList->CopyTextureRegion(&destinationLocation, 0, 0, 0, &sourceLocation, nullptr);
	}
}
//=============================================================================
GraphicsContext::GraphicsContext() : Context(D3D12_COMMAND_LIST_TYPE_DIRECT)
{
}
//=============================================================================
void GraphicsContext::SetDefaultViewPortAndScissor(glm::ivec2 screenSize)
{
	D3D12_VIEWPORT viewPort;
	viewPort.Width = static_cast<float>(screenSize.x);
	viewPort.Height = static_cast<float>(screenSize.y);
	viewPort.MinDepth = 0.0f;
	viewPort.MaxDepth = 1.0f;
	viewPort.TopLeftX = 0;
	viewPort.TopLeftY = 0;

	D3D12_RECT scissor;
	scissor.top = 0;
	scissor.left = 0;
	scissor.bottom = screenSize.y;
	scissor.right = screenSize.x;

	SetViewport(viewPort);
	SetScissorRect(scissor);
}
//=============================================================================
void GraphicsContext::SetViewport(const D3D12_VIEWPORT& viewPort)
{
	m_commandList->RSSetViewports(1, &viewPort);
}
//=============================================================================
void GraphicsContext::SetScissorRect(const D3D12_RECT& rect)
{
	m_commandList->RSSetScissorRects(1, &rect);
}
//=============================================================================
void GraphicsContext::SetStencilRef(uint32_t stencilRef)
{
	m_commandList->OMSetStencilRef(stencilRef);
}
//=============================================================================
void GraphicsContext::SetBlendFactor(glm::vec4 blendFactor)
{
	float color[4] = { blendFactor.x, blendFactor.y, blendFactor.z, blendFactor.w };
	m_commandList->OMSetBlendFactor(color);
}
//=============================================================================
void GraphicsContext::SetPrimitiveTopology(D3D12_PRIMITIVE_TOPOLOGY topology)
{
	m_commandList->IASetPrimitiveTopology(topology);
}
//=============================================================================
void GraphicsContext::SetPipeline(const PipelineInfo& pipelineBinding)
{
	const bool pipelineExpectedBoundExternally = !pipelineBinding.pipeline; //imgui

	if (!pipelineExpectedBoundExternally)
	{
		if (pipelineBinding.pipeline->pipelineType == PipelineType::compute)
		{
			m_commandList->SetPipelineState(pipelineBinding.pipeline->pipeline);
			m_commandList->SetComputeRootSignature(pipelineBinding.pipeline->rootSignature);
		}
		else
		{
			m_commandList->SetPipelineState(pipelineBinding.pipeline->pipeline);
			m_commandList->SetGraphicsRootSignature(pipelineBinding.pipeline->rootSignature);
		}
	}

	m_currentPipeline = pipelineBinding.pipeline;

	if (pipelineExpectedBoundExternally || m_currentPipeline->pipelineType == PipelineType::graphics)
	{
		D3D12_CPU_DESCRIPTOR_HANDLE renderTargetHandles[D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT]{};
		D3D12_CPU_DESCRIPTOR_HANDLE depthStencilHandle{ 0 };

		size_t renderTargetCount = pipelineBinding.renderTargets.size();

		for (size_t targetIndex = 0; targetIndex < renderTargetCount; targetIndex++)
		{
			renderTargetHandles[targetIndex] = pipelineBinding.renderTargets[targetIndex]->RTVDescriptor.CPUHandle;
		}

		if (pipelineBinding.depthStencilTarget)
		{
			depthStencilHandle = pipelineBinding.depthStencilTarget->DSVDescriptor.CPUHandle;
		}

		setTargets(static_cast<uint32_t>(renderTargetCount), renderTargetHandles, depthStencilHandle);
	}
}
//=============================================================================
void GraphicsContext::SetPipelineResources(uint32_t spaceId, const PipelineResourceSpace& resources)
{
	assert(m_currentPipeline);
	assert(resources.IsLocked());

	static const uint32_t maxNumHandlesPerBinding = 16;
	static const uint32_t singleDescriptorRangeCopyArray[maxNumHandlesPerBinding]{ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 ,1 };

	const BufferResource* cbv = resources.GetCBV();
	const auto& uavs = resources.GetUAVs();
	const auto& srvs = resources.GetSRVs();
	const uint32_t numTableHandles = static_cast<uint32_t>(uavs.size() + srvs.size());
	D3D12_CPU_DESCRIPTOR_HANDLE handles[maxNumHandlesPerBinding]{};
	uint32_t currentHandleIndex = 0;
	assert(numTableHandles <= maxNumHandlesPerBinding);

	if (cbv)
	{
		auto& cbvMapping = m_currentPipeline->pipelineResourceMapping.cbvMapping[spaceId];
		assert(cbvMapping.has_value());

		switch (m_currentPipeline->pipelineType)
		{
		case PipelineType::graphics:
			m_commandList->SetGraphicsRootConstantBufferView(cbvMapping.value(), cbv->virtualAddress);
			break;
		case PipelineType::compute:
			m_commandList->SetComputeRootConstantBufferView(cbvMapping.value(), cbv->virtualAddress);
			break;
		default:
			assert(false);
			break;
		}
	}

	if (numTableHandles == 0)
	{
		return;
	}

	for (auto& uav : uavs)
	{
		if (uav.resource->type == GPUResourceType::buffer)
		{
			handles[currentHandleIndex++] = static_cast<BufferResource*>(uav.resource)->UAVDescriptor.CPUHandle;
		}
		else
		{
			handles[currentHandleIndex++] = static_cast<TextureResource*>(uav.resource)->UAVDescriptor.CPUHandle;
		}
	}

	for (auto& srv : srvs)
	{
		if (srv.resource->type == GPUResourceType::buffer)
		{
			handles[currentHandleIndex++] = static_cast<BufferResource*>(srv.resource)->SRVDescriptor.CPUHandle;
		}
		else
		{
			handles[currentHandleIndex++] = static_cast<TextureResource*>(srv.resource)->SRVDescriptor.CPUHandle;
		}
	}

	Descriptor blockStart = m_currentSRVHeap->AllocateUserDescriptorBlock(numTableHandles);
	CopyDescriptors(1, &blockStart.CPUHandle, &numTableHandles, numTableHandles, handles, singleDescriptorRangeCopyArray, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	auto& tableMapping = m_currentPipeline->pipelineResourceMapping.tableMapping[spaceId];
	assert(tableMapping.has_value());

	switch (m_currentPipeline->pipelineType)
	{
	case PipelineType::graphics:
		m_commandList->SetGraphicsRootDescriptorTable(tableMapping.value(), blockStart.GPUHandle);
		break;
	case PipelineType::compute:
		m_commandList->SetComputeRootDescriptorTable(tableMapping.value(), blockStart.GPUHandle);
		break;
	default:
		assert(false);
		break;
	}
}
//=============================================================================
void GraphicsContext::setTargets(uint32_t numRenderTargets, const D3D12_CPU_DESCRIPTOR_HANDLE renderTargets[], D3D12_CPU_DESCRIPTOR_HANDLE depthStencil)
{
	m_commandList->OMSetRenderTargets(numRenderTargets, renderTargets, false, depthStencil.ptr != 0 ? &depthStencil : nullptr);
}
//=============================================================================
void GraphicsContext::SetIndexBuffer(const BufferResource& indexBuffer)
{
	D3D12_INDEX_BUFFER_VIEW indexBufferView;
	indexBufferView.Format = indexBuffer.stride == 4 ? DXGI_FORMAT_R32_UINT : indexBuffer.stride == 2 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_UNKNOWN;
	indexBufferView.SizeInBytes = static_cast<uint32_t>(indexBuffer.desc.Width);
	indexBufferView.BufferLocation = indexBuffer.resource->GetGPUVirtualAddress();

	m_commandList->IASetIndexBuffer(&indexBufferView);
}
//=============================================================================
void GraphicsContext::ClearRenderTarget(const TextureResource& target, glm::vec4 color)
{
	m_commandList->ClearRenderTargetView(target.RTVDescriptor.CPUHandle, &color[0], 0, nullptr);
}
//=============================================================================
void GraphicsContext::ClearDepthStencilTarget(const TextureResource& target, float depth, uint8_t stencil)
{
	m_commandList->ClearDepthStencilView(target.DSVDescriptor.CPUHandle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, depth, stencil, 0, nullptr);
}
//=============================================================================
void GraphicsContext::DrawFullScreenTriangle()
{
	SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	m_commandList->IASetIndexBuffer(nullptr);
	Draw(3);
}
//=============================================================================
void GraphicsContext::Draw(uint32_t vertexCount, uint32_t vertexStartOffset)
{
	DrawInstanced(vertexCount, 1, vertexStartOffset, 0);
}
//=============================================================================
void GraphicsContext::DrawIndexed(uint32_t indexCount, uint32_t startIndexLocation, uint32_t baseVertexLocation)
{
	DrawIndexedInstanced(indexCount, 1, startIndexLocation, baseVertexLocation, 0);
}
//=============================================================================
void GraphicsContext::DrawInstanced(uint32_t vertexCountPerInstance, uint32_t instanceCount, uint32_t startVertexLocation, uint32_t startInstanceLocation)
{
	m_commandList->DrawInstanced(vertexCountPerInstance, instanceCount, startVertexLocation, startInstanceLocation);
}
//=============================================================================
void GraphicsContext::DrawIndexedInstanced(uint32_t indexCountPerInstance, uint32_t instanceCount, uint32_t startIndexLocation, uint32_t baseVertexLocation, uint32_t startInstanceLocation)
{
	m_commandList->DrawIndexedInstanced(indexCountPerInstance, instanceCount, startIndexLocation, baseVertexLocation, startInstanceLocation);
}
//=============================================================================
void GraphicsContext::Dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ)
{
	m_commandList->Dispatch(groupCountX, groupCountY, groupCountZ);
}
//=============================================================================
void GraphicsContext::Dispatch1D(uint32_t threadCountX, uint32_t groupSizeX)
{
	Dispatch(GetGroupCount(threadCountX, groupSizeX), 1, 1);
}
//=============================================================================
void GraphicsContext::Dispatch2D(uint32_t threadCountX, uint32_t threadCountY, uint32_t groupSizeX, uint32_t groupSizeY)
{
	Dispatch(GetGroupCount(threadCountX, groupSizeX), GetGroupCount(threadCountY, groupSizeY), 1);
}
//=============================================================================
void GraphicsContext::Dispatch3D(uint32_t threadCountX, uint32_t threadCountY, uint32_t threadCountZ, uint32_t groupSizeX, uint32_t groupSizeY, uint32_t groupSizeZ)
{
	Dispatch(GetGroupCount(threadCountX, groupSizeX), GetGroupCount(threadCountY, groupSizeY), GetGroupCount(threadCountZ, groupSizeZ));
}
//=============================================================================
ComputeContext::ComputeContext() : Context(D3D12_COMMAND_LIST_TYPE_COMPUTE)
{
}
//=============================================================================
void ComputeContext::SetPipeline(const PipelineInfo& pipelineBinding)
{
	assert(pipelineBinding.pipeline && pipelineBinding.pipeline->pipelineType == PipelineType::compute);

	m_commandList->SetPipelineState(pipelineBinding.pipeline->pipeline);
	m_commandList->SetComputeRootSignature(pipelineBinding.pipeline->rootSignature);

	m_currentPipeline = pipelineBinding.pipeline;
}
//=============================================================================
void ComputeContext::SetPipelineResources(uint32_t spaceId, const PipelineResourceSpace& resources)
{
	assert(m_currentPipeline);
	assert(resources.IsLocked());

	static const uint32_t maxNumHandlesPerBinding = 16;
	static const uint32_t singleDescriptorRangeCopyArray[maxNumHandlesPerBinding]{ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 ,1 };

	const BufferResource* cbv = resources.GetCBV();
	const auto& uavs = resources.GetUAVs();
	const auto& srvs = resources.GetSRVs();
	const uint32_t numTableHandles = static_cast<uint32_t>(uavs.size() + srvs.size());
	D3D12_CPU_DESCRIPTOR_HANDLE handles[maxNumHandlesPerBinding]{};
	uint32_t currentHandleIndex = 0;

	assert(numTableHandles <= maxNumHandlesPerBinding);

	if (cbv)
	{
		auto& cbvMapping = m_currentPipeline->pipelineResourceMapping.cbvMapping[spaceId];
		assert(cbvMapping.has_value());

		m_commandList->SetComputeRootConstantBufferView(cbvMapping.value(), cbv->virtualAddress);
	}

	if (numTableHandles == 0)
	{
		return;
	}

	for (auto& uav : uavs)
	{
		if (uav.resource->type == GPUResourceType::buffer)
		{
			handles[currentHandleIndex++] = static_cast<BufferResource*>(uav.resource)->UAVDescriptor.CPUHandle;
		}
		else
		{
			handles[currentHandleIndex++] = static_cast<TextureResource*>(uav.resource)->UAVDescriptor.CPUHandle;
		}
	}

	for (auto& srv : srvs)
	{
		if (srv.resource->type == GPUResourceType::buffer)
		{
			handles[currentHandleIndex++] = static_cast<BufferResource*>(srv.resource)->SRVDescriptor.CPUHandle;
		}
		else
		{
			handles[currentHandleIndex++] = static_cast<TextureResource*>(srv.resource)->SRVDescriptor.CPUHandle;
		}
	}

	Descriptor blockStart = m_currentSRVHeap->AllocateUserDescriptorBlock(numTableHandles);
	CopyDescriptors(1, &blockStart.CPUHandle, &numTableHandles, numTableHandles, handles, singleDescriptorRangeCopyArray, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	auto& tableMapping = m_currentPipeline->pipelineResourceMapping.tableMapping[spaceId];
	assert(tableMapping.has_value());

	m_commandList->SetComputeRootDescriptorTable(tableMapping.value(), blockStart.GPUHandle);
}
//=============================================================================
void ComputeContext::Dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ)
{
	m_commandList->Dispatch(groupCountX, groupCountY, groupCountZ);
}
//=============================================================================
void ComputeContext::Dispatch1D(uint32_t threadCountX, uint32_t groupSizeX)
{
	Dispatch(GetGroupCount(threadCountX, groupSizeX), 1, 1);
}
//=============================================================================
void ComputeContext::Dispatch2D(uint32_t threadCountX, uint32_t threadCountY, uint32_t groupSizeX, uint32_t groupSizeY)
{
	Dispatch(GetGroupCount(threadCountX, groupSizeX), GetGroupCount(threadCountY, groupSizeY), 1);
}
//=============================================================================
void ComputeContext::Dispatch3D(uint32_t threadCountX, uint32_t threadCountY, uint32_t threadCountZ, uint32_t groupSizeX, uint32_t groupSizeY, uint32_t groupSizeZ)
{
	Dispatch(GetGroupCount(threadCountX, groupSizeX), GetGroupCount(threadCountY, groupSizeY), GetGroupCount(threadCountZ, groupSizeZ));
}
//=============================================================================
UploadContext::UploadContext(std::unique_ptr<BufferResource> bufferUploadHeap, std::unique_ptr<BufferResource> textureUploadHeap)
	: Context(D3D12_COMMAND_LIST_TYPE_COPY)
	, m_bufferUploadHeap(std::move(bufferUploadHeap))
	, m_textureUploadHeap(std::move(textureUploadHeap))
{
}
//=============================================================================
UploadContext::~UploadContext()
{
	//Upload context heaps weren't returned for some reason
	assert(m_bufferUploadHeap == nullptr);
	assert(m_textureUploadHeap == nullptr);
}
//=============================================================================
std::unique_ptr<BufferResource> UploadContext::ReturnBufferHeap()
{
	return std::move(m_bufferUploadHeap);
}
//=============================================================================
std::unique_ptr<BufferResource> UploadContext::ReturnTextureHeap()
{
	return std::move(m_textureUploadHeap);
}
//=============================================================================
void UploadContext::AddBufferUpload(std::unique_ptr<BufferUpload> bufferUpload)
{
	assert(bufferUpload->bufferDataSize <= m_bufferUploadHeap->desc.Width);

	m_bufferUploads.push_back(std::move(bufferUpload));
}
//=============================================================================
void UploadContext::AddTextureUpload(std::unique_ptr<TextureUpload> textureUpload)
{
	assert(textureUpload->textureDataSize <= m_textureUploadHeap->desc.Width);

	m_textureUploads.push_back(std::move(textureUpload));
}
//=============================================================================
void UploadContext::ProcessUploads()
{
	const uint32_t numBufferUploads = static_cast<uint32_t>(m_bufferUploads.size());
	const uint32_t numTextureUploads = static_cast<uint32_t>(m_textureUploads.size());
	uint32_t numBuffersProcessed = 0;
	uint32_t numTexturesProcessed = 0;
	size_t bufferUploadHeapOffset = 0;
	size_t textureUploadHeapOffset = 0;

	for (numBuffersProcessed; numBuffersProcessed < numBufferUploads; numBuffersProcessed++)
	{
		BufferUpload& currentUpload = *m_bufferUploads[numBuffersProcessed];

		if ((bufferUploadHeapOffset + currentUpload.bufferDataSize) > m_bufferUploadHeap->desc.Width)
		{
			break;
		}

		memcpy(m_bufferUploadHeap->mappedResource + bufferUploadHeapOffset, currentUpload.bufferData.get(), currentUpload.bufferDataSize);
		CopyBufferRegion(*currentUpload.buffer, 0, *m_bufferUploadHeap, bufferUploadHeapOffset, currentUpload.bufferDataSize);

		bufferUploadHeapOffset += currentUpload.bufferDataSize;
		m_bufferUploadsInProgress.push_back(currentUpload.buffer);
	}

	for (numTexturesProcessed; numTexturesProcessed < numTextureUploads; numTexturesProcessed++)
	{
		TextureUpload& currentUpload = *m_textureUploads[numTexturesProcessed];

		if ((textureUploadHeapOffset + currentUpload.textureDataSize) > m_textureUploadHeap->desc.Width)
		{
			break;
		}

		memcpy(m_textureUploadHeap->mappedResource + textureUploadHeapOffset, currentUpload.textureData.get(), currentUpload.textureDataSize);
		CopyTextureRegion(*currentUpload.texture, *m_textureUploadHeap, textureUploadHeapOffset, currentUpload.subResourceLayouts, currentUpload.numSubResources);

		textureUploadHeapOffset += currentUpload.textureDataSize;
		textureUploadHeapOffset = AlignU64(textureUploadHeapOffset, 512);

		m_textureUploadsInProgress.push_back(currentUpload.texture);
	}

	if (numBuffersProcessed > 0)
	{
		m_bufferUploads.erase(m_bufferUploads.begin(), m_bufferUploads.begin() + numBuffersProcessed);
	}

	if (numTexturesProcessed > 0)
	{
		m_textureUploads.erase(m_textureUploads.begin(), m_textureUploads.begin() + numTexturesProcessed);
	}
}
//=============================================================================
void UploadContext::ResolveProcessedUploads()
{
	for (auto& bufferUploadInProgress : m_bufferUploadsInProgress)
	{
		bufferUploadInProgress->isReady = true;
	}

	for (auto& textureUploadInProgress : m_textureUploadsInProgress)
	{
		textureUploadInProgress->isReady = true;
	}

	m_bufferUploadsInProgress.clear();
	m_textureUploadsInProgress.clear();
}
//=============================================================================
#endif // RENDER_D3D12