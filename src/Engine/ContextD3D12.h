#pragma once

#if RENDER_D3D12

#include "DescriptorHeapD3D12.h"

// TODO: rename Resource pr Loader?

class Context
{
public:
	Context(D3D12_COMMAND_LIST_TYPE commandType);
	virtual ~Context();

	D3D12_COMMAND_LIST_TYPE GetCommandType() { return mContextType; }
	ID3D12GraphicsCommandList* GetCommandList() { return mCommandList; }

	void Reset();
	void AddBarrier(Resource& resource, D3D12_RESOURCE_STATES newState);
	void FlushBarriers();
	void CopyResource(const Resource& destination, const Resource& source);
	void CopyBufferRegion(Resource& destination, uint64_t destOffset, Resource& source, uint64_t sourceOffset, uint64_t numBytes);
	void CopyTextureRegion(Resource& destination, Resource& source, size_t sourceOffset, SubResourceLayouts& subResourceLayouts, uint32_t numSubResources);

protected:
	void BindDescriptorHeaps(uint32_t frameIndex);

	D3D12_COMMAND_LIST_TYPE mContextType = D3D12_COMMAND_LIST_TYPE_DIRECT;
	ID3D12GraphicsCommandList4* mCommandList = nullptr;
	std::array<ID3D12DescriptorHeap*, D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES> mCurrentDescriptorHeaps{ nullptr };
	std::array<ID3D12CommandAllocator*, NUM_FRAMES_IN_FLIGHT> mCommandAllocators{ nullptr };
	std::array<D3D12_RESOURCE_BARRIER, MAX_QUEUED_BARRIERS> mResourceBarriers{};
	uint32_t mNumQueuedBarriers = 0;
	RenderPassDescriptorHeap* mCurrentSRVHeap = nullptr;
	D3D12_CPU_DESCRIPTOR_HANDLE mCurrentSRVHeapHandle{ 0 };
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
	std::vector<std::unique_ptr<BufferUpload>>  mBufferUploads;
	std::vector<std::unique_ptr<TextureUpload>> mTextureUploads;
	std::vector<BufferResource*>                mBufferUploadsInProgress;
	std::vector<TextureResource*>               mTextureUploadsInProgress;
	std::unique_ptr<BufferResource>             mBufferUploadHeap;
	std::unique_ptr<BufferResource>             mTextureUploadHeap;
};

#endif // RENDER_D3D12