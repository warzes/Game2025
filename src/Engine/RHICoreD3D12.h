#pragma once

#if RENDER_D3D12

constexpr uint32_t MAX_BACK_BUFFER_COUNT = 3;
constexpr uint32_t NUM_RTV_STAGING_DESCRIPTORS = 256;
constexpr uint32_t NUM_DSV_STAGING_DESCRIPTORS = 32;
constexpr uint32_t NUM_SRV_STAGING_DESCRIPTORS = 4096;

constexpr uint32_t INVALID_RESOURCE_TABLE_INDEX = UINT_MAX;

struct RenderFeatures final
{
	bool allowTearing{ false };
};

template<class... Args>
void SetName(ID3D12Object* pObj, const char* format, Args&&... args)
{
	char bufName[240];
	sprintf_s(bufName, format, args...);
	std::string Name = bufName;
	std::wstring wName(Name.begin(), Name.end());
	pObj->SetName(wName.c_str());
}

const std::string DXErrorToStr(HRESULT hr);
const std::string ConvertToStr(D3D_FEATURE_LEVEL level);

struct DescriptorD3D12 final
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

	DescriptorD3D12 Descriptor{};
};

#endif // RENDER_D3D12