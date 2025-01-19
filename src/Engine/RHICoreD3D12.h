#pragma once

#if RENDER_D3D12

#include "Log.h"

constexpr uint32_t MAX_BACK_BUFFER_COUNT = 3;
constexpr uint32_t NUM_RTV_STAGING_DESCRIPTORS = 64;
constexpr uint32_t NUM_DSV_STAGING_DESCRIPTORS = 16;
constexpr uint32_t NUM_SRV_STAGING_DESCRIPTORS = 4096;
constexpr uint32_t NUM_SAMPLER_DESCRIPTORS = 6;
constexpr uint32_t NUM_RESERVED_SRV_DESCRIPTORS = 8192;
constexpr uint32_t NUM_SRV_RENDER_PASS_USER_DESCRIPTORS = 65536;

constexpr uint32_t INVALID_RESOURCE_TABLE_INDEX = UINT_MAX;

const std::string DXErrorToStr(HRESULT hr);
const std::string ConvertToStr(D3D_FEATURE_LEVEL level);

struct DescriptorHandleD3D12 final
{
	bool IsValid() const { return CPUHandle.ptr != 0; }
	bool IsReferencedByShader() const { return GPUHandle.ptr != 0; }

	D3D12_CPU_DESCRIPTOR_HANDLE CPUHandle{ 0 };
	D3D12_GPU_DESCRIPTOR_HANDLE GPUHandle{ 0 };
	uint32_t                    heapIndex{ 0 };
};

enum class GPUResourceType
{
	Buffer,
	Texture
};

struct ResourceD3D12
{
	GPUResourceType             type{ GPUResourceType::Buffer };
	ComPtr<ID3D12Resource>      resource{ nullptr };
	ComPtr<D3D12MA::Allocation> allocation{ nullptr };
	D3D12_GPU_VIRTUAL_ADDRESS   virtualAddress{ 0 };
	D3D12_RESOURCE_STATES       state{ D3D12_RESOURCE_STATE_COMMON };
	bool                        isReady{ false };
	uint32_t                    descriptorHeapIndex{ INVALID_RESOURCE_TABLE_INDEX };
};

struct TextureResourceD3D12 final : public ResourceD3D12
{
	TextureResourceD3D12() : ResourceD3D12()
	{
		type = GPUResourceType::Texture;
	}

	void Destroy()
	{
		resource.Reset();
		allocation.Reset();
		isReady = false;
	}

	DescriptorHandleD3D12 Descriptor{};
};

#endif // RENDER_D3D12