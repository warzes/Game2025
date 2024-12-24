#include "stdafx.h"
#if RENDER_D3D12
#include "ContextD3D12.h"
#include "RenderContextD3D12.h"
#include "Log.h"
https://alextardif.com/DX12Tutorial.html
//=============================================================================
Context::Context(D3D12_COMMAND_LIST_TYPE commandType) : mContextType(commandType)
{
	HRESULT result;
	for (uint32_t frameIndex = 0; frameIndex < NUM_FRAMES_IN_FLIGHT; frameIndex++)
	{
		result = gRenderContext.device->CreateCommandAllocator(commandType, IID_PPV_ARGS(&mCommandAllocators[frameIndex]));
		if (FAILED(result))
		{
			Fatal("ID3D12Device::CreateCommandAllocator() failed: " + DXErrorToStr(result));
			return;
		}
	}

	result = gRenderContext.device->CreateCommandList1(0, commandType, D3D12_COMMAND_LIST_FLAG_NONE, IID_PPV_ARGS(&mCommandList));
	if (FAILED(result))
	{
		Fatal("ID3D12Device::CreateCommandList1() failed: " + DXErrorToStr(result));
		return;
	}
}
//=============================================================================
Context::~Context()
{
	SafeRelease(mCommandList);

	for (uint32_t frameIndex = 0; frameIndex < NUM_FRAMES_IN_FLIGHT; frameIndex++)
	{
		SafeRelease(mCommandAllocators[frameIndex]);
	}
}
//=============================================================================
void Context::Reset()
{
	uint32_t frameId = mDevice.GetFrameId();

	mCommandAllocators[frameId]->Reset();
	mCommandList->Reset(mCommandAllocators[frameId], nullptr);

	if (mContextType != D3D12_COMMAND_LIST_TYPE_COPY)
	{
		BindDescriptorHeaps(mDevice.GetFrameId());
	}
}
//=============================================================================
void Context::AddBarrier(Resource& resource, D3D12_RESOURCE_STATES newState)
{
	if (mNumQueuedBarriers >= MAX_QUEUED_BARRIERS)
	{
		FlushBarriers();
	}

	D3D12_RESOURCE_STATES oldState = resource.state;

	if (mContextType == D3D12_COMMAND_LIST_TYPE_COMPUTE)
	{
		constexpr D3D12_RESOURCE_STATES VALID_COMPUTE_CONTEXT_STATES = (D3D12_RESOURCE_STATE_UNORDERED_ACCESS | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE |
			D3D12_RESOURCE_STATE_COPY_DEST | D3D12_RESOURCE_STATE_COPY_SOURCE);

		assert((oldState & VALID_COMPUTE_CONTEXT_STATES) == oldState);
		assert((newState & VALID_COMPUTE_CONTEXT_STATES) == newState);
	}

	if (oldState != newState)
	{
		D3D12_RESOURCE_BARRIER& barrierDesc = mResourceBarriers[mNumQueuedBarriers];
		mNumQueuedBarriers++;

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
		D3D12_RESOURCE_BARRIER& barrierDesc = mResourceBarriers[mNumQueuedBarriers];
		mNumQueuedBarriers++;

		barrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
		barrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		barrierDesc.UAV.pResource = resource.resource;
	}
}
//=============================================================================
void Context::FlushBarriers()
{
	if (mNumQueuedBarriers > 0)
	{
		mCommandList->ResourceBarrier(mNumQueuedBarriers, mResourceBarriers.data());
		mNumQueuedBarriers = 0;
	}
}
//=============================================================================
void Context::BindDescriptorHeaps(uint32_t frameIndex)
{
	mCurrentSRVHeap = &mDevice.GetSRVHeap(frameIndex);
	mCurrentSRVHeap->Reset();

	ID3D12DescriptorHeap* heapsToBind[2];
	heapsToBind[0] = mDevice.GetSRVHeap(frameIndex).GetHeap();
	heapsToBind[1] = mDevice.GetSamplerHeap().GetHeap();

	mCommandList->SetDescriptorHeaps(2, heapsToBind);
}
//=============================================================================
void Context::CopyResource(const Resource& destination, const Resource& source)
{
	mCommandList->CopyResource(destination.resource, source.resource);
}
//=============================================================================
void Context::CopyBufferRegion(Resource& destination, uint64_t destOffset, Resource& source, uint64_t sourceOffset, uint64_t numBytes)
{
	mCommandList->CopyBufferRegion(destination.resource, destOffset, source.resource, sourceOffset, numBytes);
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

		mCommandList->CopyTextureRegion(&destinationLocation, 0, 0, 0, &sourceLocation, nullptr);
	}
}
//=============================================================================
UploadContext::UploadContext(std::unique_ptr<BufferResource> bufferUploadHeap, std::unique_ptr<BufferResource> textureUploadHeap)
	: Context(D3D12_COMMAND_LIST_TYPE_COPY)
	, mBufferUploadHeap(std::move(bufferUploadHeap))
	, mTextureUploadHeap(std::move(textureUploadHeap))
{
}
//=============================================================================
UploadContext::~UploadContext()
{
	//Upload context heaps weren't returned for some reason
	assert(mBufferUploadHeap == nullptr);
	assert(mTextureUploadHeap == nullptr);
}
//=============================================================================
std::unique_ptr<BufferResource> UploadContext::ReturnBufferHeap()
{
	return std::move(mBufferUploadHeap);
}
//=============================================================================
std::unique_ptr<BufferResource> UploadContext::ReturnTextureHeap()
{
	return std::move(mTextureUploadHeap);
}
//=============================================================================
void UploadContext::AddBufferUpload(std::unique_ptr<BufferUpload> bufferUpload)
{
	assert(bufferUpload->bufferDataSize <= mBufferUploadHeap->desc.Width);

	mBufferUploads.push_back(std::move(bufferUpload));
}
//=============================================================================
void UploadContext::AddTextureUpload(std::unique_ptr<TextureUpload> textureUpload)
{
	assert(textureUpload->textureDataSize <= mTextureUploadHeap->desc.Width);

	mTextureUploads.push_back(std::move(textureUpload));
}
//=============================================================================
void UploadContext::ProcessUploads()
{
	const uint32_t numBufferUploads = static_cast<uint32_t>(mBufferUploads.size());
	const uint32_t numTextureUploads = static_cast<uint32_t>(mTextureUploads.size());
	uint32_t numBuffersProcessed = 0;
	uint32_t numTexturesProcessed = 0;
	size_t bufferUploadHeapOffset = 0;
	size_t textureUploadHeapOffset = 0;

	for (numBuffersProcessed; numBuffersProcessed < numBufferUploads; numBuffersProcessed++)
	{
		BufferUpload& currentUpload = *mBufferUploads[numBuffersProcessed];

		if ((bufferUploadHeapOffset + currentUpload.bufferDataSize) > mBufferUploadHeap->desc.Width)
		{
			break;
		}

		memcpy(mBufferUploadHeap->mMappedResource + bufferUploadHeapOffset, currentUpload.mBufferData.get(), currentUpload.mBufferDataSize);
		CopyBufferRegion(*currentUpload.mBuffer, 0, *mBufferUploadHeap, bufferUploadHeapOffset, currentUpload.mBufferDataSize);

		bufferUploadHeapOffset += currentUpload.mBufferDataSize;
		mBufferUploadsInProgress.push_back(currentUpload.mBuffer);
	}

	for (numTexturesProcessed; numTexturesProcessed < numTextureUploads; numTexturesProcessed++)
	{
		TextureUpload& currentUpload = *mTextureUploads[numTexturesProcessed];

		if ((textureUploadHeapOffset + currentUpload.mTextureDataSize) > mTextureUploadHeap->mDesc.Width)
		{
			break;
		}

		memcpy(mTextureUploadHeap->mMappedResource + textureUploadHeapOffset, currentUpload.mTextureData.get(), currentUpload.mTextureDataSize);
		CopyTextureRegion(*currentUpload.mTexture, *mTextureUploadHeap, textureUploadHeapOffset, currentUpload.mSubResourceLayouts, currentUpload.mNumSubResources);

		textureUploadHeapOffset += currentUpload.mTextureDataSize;
		textureUploadHeapOffset = AlignU64(textureUploadHeapOffset, 512);

		mTextureUploadsInProgress.push_back(currentUpload.mTexture);
	}

	if (numBuffersProcessed > 0)
	{
		mBufferUploads.erase(mBufferUploads.begin(), mBufferUploads.begin() + numBuffersProcessed);
	}

	if (numTexturesProcessed > 0)
	{
		mTextureUploads.erase(mTextureUploads.begin(), mTextureUploads.begin() + numTexturesProcessed);
	}
}
//=============================================================================
void UploadContext::ResolveProcessedUploads()
{
	for (auto& bufferUploadInProgress : mBufferUploadsInProgress)
	{
		bufferUploadInProgress->mIsReady = true;
	}

	for (auto& textureUploadInProgress : mTextureUploadsInProgress)
	{
		textureUploadInProgress->mIsReady = true;
	}

	mBufferUploadsInProgress.clear();
	mTextureUploadsInProgress.clear();
}
//=============================================================================
#endif // RENDER_D3D12