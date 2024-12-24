#pragma once

#if RENDER_D3D12

#include "DescriptorHeapD3D12.h"

// TODO: RenderContext?

class Context
{
public:
	Context(D3D12_COMMAND_LIST_TYPE commandType);
	virtual ~Context();

	D3D12_COMMAND_LIST_TYPE GetCommandType() { return m_contextType; }
	ID3D12GraphicsCommandList* GetCommandList() { return m_commandList; }

	void Reset();
	void AddBarrier(Resource& resource, D3D12_RESOURCE_STATES newState);
	void FlushBarriers();
	void CopyResource(const Resource& destination, const Resource& source);
	void CopyBufferRegion(Resource& destination, uint64_t destOffset, Resource& source, uint64_t sourceOffset, uint64_t numBytes);
	void CopyTextureRegion(Resource& destination, Resource& source, size_t sourceOffset, SubResourceLayouts& subResourceLayouts, uint32_t numSubResources);

protected:
	void bindDescriptorHeaps(uint32_t frameIndex);

	D3D12_COMMAND_LIST_TYPE                                                 m_contextType{ D3D12_COMMAND_LIST_TYPE_DIRECT };
	ID3D12GraphicsCommandList4*                                             m_commandList{ nullptr };
	std::array<ID3D12DescriptorHeap*, D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES> m_currentDescriptorHeaps{ nullptr };
	std::array<ID3D12CommandAllocator*, NUM_FRAMES_IN_FLIGHT>               m_commandAllocators{ nullptr };
	std::array<D3D12_RESOURCE_BARRIER, MAX_QUEUED_BARRIERS>                 m_resourceBarriers{};
	uint32_t                                                                m_numQueuedBarriers{ 0 };
	RenderPassDescriptorHeap*                                               m_currentSRVHeap{ nullptr };
	D3D12_CPU_DESCRIPTOR_HANDLE                                             m_currentSRVHeapHandle{ 0 };
};

class GraphicsContext final : public Context
{
public:
	GraphicsContext();

	void SetDefaultViewPortAndScissor(glm::ivec2 screenSize);
	void SetViewport(const D3D12_VIEWPORT& viewPort);
	void SetScissorRect(const D3D12_RECT& rect);
	void SetStencilRef(uint32_t stencilRef);
	void SetBlendFactor(glm::vec4 blendFactor);
	void SetPrimitiveTopology(D3D12_PRIMITIVE_TOPOLOGY topology);
	void SetPipeline(const PipelineInfo& pipelineBinding);
	void SetPipelineResources(uint32_t spaceId, const PipelineResourceSpace& resources);
	void SetIndexBuffer(const BufferResource& indexBuffer);
	void ClearRenderTarget(const TextureResource& target, glm::vec4 color);
	void ClearDepthStencilTarget(const TextureResource& target, float depth, uint8_t stencil);
	void DrawFullScreenTriangle();
	void Draw(uint32_t vertexCount, uint32_t vertexStartOffset = 0);
	void DrawIndexed(uint32_t indexCount, uint32_t startIndexLocation = 0, uint32_t baseVertexLocation = 0);
	void DrawInstanced(uint32_t vertexCountPerInstance, uint32_t instanceCount, uint32_t startVertexLocation = 0, uint32_t startInstanceLocation = 0);
	void DrawIndexedInstanced(uint32_t indexCountPerInstance, uint32_t instanceCount, uint32_t startIndexLocation, uint32_t baseVertexLocation, uint32_t startInstanceLocation);
	void Dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ);
	void Dispatch1D(uint32_t threadCountX, uint32_t groupSizeX);
	void Dispatch2D(uint32_t threadCountX, uint32_t threadCountY, uint32_t groupSizeX, uint32_t groupSizeY);
	void Dispatch3D(uint32_t threadCountX, uint32_t threadCountY, uint32_t threadCountZ, uint32_t groupSizeX, uint32_t groupSizeY, uint32_t groupSizeZ);

private:
	void setTargets(uint32_t numRenderTargets, const D3D12_CPU_DESCRIPTOR_HANDLE renderTargets[], D3D12_CPU_DESCRIPTOR_HANDLE depthStencil);

	PipelineStateObject* m_currentPipeline{ nullptr };
};

class ComputeContext final : public Context
{
public:
	ComputeContext();

	void SetPipeline(const PipelineInfo& pipelineBinding);
	void SetPipelineResources(uint32_t spaceId, const PipelineResourceSpace& resources);
	void Dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ);
	void Dispatch1D(uint32_t threadCountX, uint32_t groupSizeX);
	void Dispatch2D(uint32_t threadCountX, uint32_t threadCountY, uint32_t groupSizeX, uint32_t groupSizeY);
	void Dispatch3D(uint32_t threadCountX, uint32_t threadCountY, uint32_t threadCountZ, uint32_t groupSizeX, uint32_t groupSizeY, uint32_t groupSizeZ);

private:
	PipelineStateObject* m_currentPipeline{ nullptr };
};

class UploadContext final : public Context
{
public:
	UploadContext(std::unique_ptr<BufferResource> bufferUploadHeap, std::unique_ptr<BufferResource> textureUploadHeap);
	~UploadContext();

	std::unique_ptr<BufferResource> ReturnBufferHeap();
	std::unique_ptr<BufferResource> ReturnTextureHeap();

	void AddBufferUpload(std::unique_ptr<BufferUpload> bufferUpload);
	void AddTextureUpload(std::unique_ptr<TextureUpload> textureUpload);
	void ProcessUploads();
	void ResolveProcessedUploads();

private:
	std::vector<std::unique_ptr<BufferUpload>>  m_bufferUploads;
	std::vector<std::unique_ptr<TextureUpload>> m_textureUploads;
	std::vector<BufferResource*>                m_bufferUploadsInProgress;
	std::vector<TextureResource*>               m_textureUploadsInProgress;
	std::unique_ptr<BufferResource>             m_bufferUploadHeap;
	std::unique_ptr<BufferResource>             m_textureUploadHeap;
};

#endif // RENDER_D3D12