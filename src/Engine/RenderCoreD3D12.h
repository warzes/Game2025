#pragma once

#if RENDER_D3D12

#include "RenderCore.h"

constexpr uint32_t NUM_FRAMES_IN_FLIGHT = 2;
constexpr uint32_t NUM_BACK_BUFFERS = 3;
constexpr uint32_t NUM_RTV_STAGING_DESCRIPTORS = 256;
constexpr uint32_t NUM_DSV_STAGING_DESCRIPTORS = 32;
constexpr uint32_t NUM_SRV_STAGING_DESCRIPTORS = 4096;
constexpr uint32_t NUM_SAMPLER_DESCRIPTORS = 6;
constexpr uint32_t NUM_RESERVED_SRV_DESCRIPTORS = 8192;
constexpr uint32_t NUM_SRV_RENDER_PASS_USER_DESCRIPTORS = 65536;
constexpr uint32_t IMGUI_RESERVED_DESCRIPTOR_INDEX = 0;
constexpr uint32_t INVALID_RESOURCE_TABLE_INDEX = UINT_MAX;
constexpr uint32_t MAX_TEXTURE_SUBRESOURCE_COUNT = 32;
constexpr uint32_t MAX_QUEUED_BARRIERS = 16;

using SubResourceLayouts = std::array<D3D12_PLACED_SUBRESOURCE_FOOTPRINT, MAX_TEXTURE_SUBRESOURCE_COUNT>;

enum class BufferViewFlags : uint8_t
{
	none = 0,
	cbv = 1,
	srv = 2,
	uav = 4
};

inline BufferViewFlags operator|(BufferViewFlags a, BufferViewFlags b)
{
	return static_cast<BufferViewFlags>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}

inline BufferViewFlags operator&(BufferViewFlags a, BufferViewFlags b)
{
	return static_cast<BufferViewFlags>(static_cast<uint8_t>(a) & static_cast<uint8_t>(b));
}

enum class BufferAccessFlags : uint8_t
{
	gpuOnly = 0,
	hostWritable = 1
};

inline BufferAccessFlags operator|(BufferAccessFlags a, BufferAccessFlags b)
{
	return static_cast<BufferAccessFlags>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}

inline BufferAccessFlags operator&(BufferAccessFlags a, BufferAccessFlags b)
{
	return static_cast<BufferAccessFlags>(static_cast<uint8_t>(a) & static_cast<uint8_t>(b));
}

enum class GPUResourceType : bool
{
	buffer = false,
	texture = true
};

struct Descriptor final
{
	bool IsValid() const { return CPUHandle.ptr != 0; }
	bool IsReferencedByShader() const { return GPUHandle.ptr != 0; }

	D3D12_CPU_DESCRIPTOR_HANDLE CPUHandle{ 0 };
	D3D12_GPU_DESCRIPTOR_HANDLE GPUHandle{ 0 };
	uint32_t                    heapIndex{ 0 };
};

struct BufferCreationDesc final
{
	uint32_t          size{ 0 };
	uint32_t          stride{ 0 };
	BufferViewFlags   viewFlags{ BufferViewFlags::none };
	BufferAccessFlags accessFlags{ BufferAccessFlags::gpuOnly };
	bool              isRawAccess{ false };
};

struct Resource
{
	GPUResourceType           type{ GPUResourceType::buffer };
	D3D12_RESOURCE_DESC       desc{};
	ID3D12Resource*           resource{ nullptr };
	D3D12MA::Allocation*      allocation{ nullptr };
	D3D12_GPU_VIRTUAL_ADDRESS virtualAddress{ 0 };
	D3D12_RESOURCE_STATES     state{ D3D12_RESOURCE_STATE_COMMON };
	bool                      isReady{ false };
	uint32_t                  descriptorHeapIndex{ INVALID_RESOURCE_TABLE_INDEX };
};

struct BufferResource final : public Resource
{
	BufferResource() : Resource()
	{
		type = GPUResourceType::buffer;
	}

	void SetMappedData(void* data, size_t dataSize)
	{
		assert(mappedResource != nullptr && data != nullptr && dataSize > 0 && dataSize <= desc.Width);
		memcpy_s(mappedResource, desc.Width, data, dataSize);
	}

	uint8_t*   mappedResource{ nullptr };
	uint32_t   stride{ 0 };
	Descriptor CBVDescriptor{};
	Descriptor SRVDescriptor{};
	Descriptor UAVDescriptor{};
};

struct TextureResource final : public Resource
{
	TextureResource() : Resource()
	{
		type = GPUResourceType::texture;
	}

	Descriptor RTVDescriptor{};
	Descriptor DSVDescriptor{};
	Descriptor SRVDescriptor{};
	Descriptor UAVDescriptor{};
};

struct BufferUpload final
{
	BufferResource*            buffer{ nullptr };
	std::unique_ptr<uint8_t[]> bufferData;
	size_t                     bufferDataSize{ 0 };
};

struct TextureUpload final
{
	TextureResource*           texture{ nullptr };
	std::unique_ptr<uint8_t[]> textureData;
	size_t                     textureDataSize{ 0 };
	uint32_t                   numSubResources{ 0 };
	SubResourceLayouts         subResourceLayouts{ 0 };
};


const std::string DXErrorToStr(HRESULT hr);

#endif // RENDER_D3D12