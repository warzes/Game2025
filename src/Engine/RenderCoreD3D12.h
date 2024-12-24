#pragma once

#if RENDER_D3D12

#include "RenderCore.h"

constexpr uint32_t NUM_FRAMES_IN_FLIGHT = 2;
constexpr uint32_t NUM_BACK_BUFFERS = 3;
constexpr uint32_t NUM_RTV_STAGING_DESCRIPTORS = 256;
constexpr uint32_t NUM_DSV_STAGING_DESCRIPTORS = 32;
constexpr uint32_t NUM_SRV_STAGING_DESCRIPTORS = 4096;
constexpr uint32_t NUM_SAMPLER_DESCRIPTORS = 6;
constexpr uint32_t MAX_QUEUED_BARRIERS = 16;
constexpr uint8_t PER_OBJECT_SPACE = 0;
constexpr uint8_t PER_MATERIAL_SPACE = 1;
constexpr uint8_t PER_PASS_SPACE = 2;
constexpr uint8_t PER_FRAME_SPACE = 3;
constexpr uint8_t NUM_RESOURCE_SPACES = 4;
constexpr uint32_t NUM_RESERVED_SRV_DESCRIPTORS = 8192;
constexpr uint32_t IMGUI_RESERVED_DESCRIPTOR_INDEX = 0;
constexpr uint32_t NUM_SRV_RENDER_PASS_USER_DESCRIPTORS = 65536;
constexpr uint32_t INVALID_RESOURCE_TABLE_INDEX = UINT_MAX;
constexpr uint32_t MAX_TEXTURE_SUBRESOURCE_COUNT = 32;
static const wchar_t* SHADER_SOURCE_PATH = L"Data/Shaders/";
static const wchar_t* SHADER_OUTPUT_PATH = L"Data/Shaders/Compiled/";
static const char* RESOURCE_PATH = "Data/Resources/";

using SubResourceLayouts = std::array<D3D12_PLACED_SUBRESOURCE_FOOTPRINT, MAX_TEXTURE_SUBRESOURCE_COUNT>;

enum class GPUResourceType : bool
{
	buffer = false,
	texture = true
};

enum class BufferAccessFlags : uint8_t
{
	gpuOnly = 0,
	hostWritable = 1
};

enum class BufferViewFlags : uint8_t
{
	none = 0,
	cbv = 1,
	srv = 2,
	uav = 4
};

enum class TextureViewFlags : uint8_t
{
	none = 0,
	rtv = 1,
	dsv = 2,
	srv = 4,
	uav = 8
};

enum class ContextWaitType : uint8_t
{
	host = 0,
	graphics,
	compute,
	copy
};

enum class PipelineType : uint8_t
{
	graphics = 0,
	compute
};

enum class ShaderType : uint8_t
{
	vertex = 0,
	pixel,
	compute
};

inline BufferAccessFlags operator|(BufferAccessFlags a, BufferAccessFlags b)
{
	return static_cast<BufferAccessFlags>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}

inline BufferAccessFlags operator&(BufferAccessFlags a, BufferAccessFlags b)
{
	return static_cast<BufferAccessFlags>(static_cast<uint8_t>(a) & static_cast<uint8_t>(b));
}

inline BufferViewFlags operator|(BufferViewFlags a, BufferViewFlags b)
{
	return static_cast<BufferViewFlags>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}

inline BufferViewFlags operator&(BufferViewFlags a, BufferViewFlags b)
{
	return static_cast<BufferViewFlags>(static_cast<uint8_t>(a) & static_cast<uint8_t>(b));
}

inline TextureViewFlags operator|(TextureViewFlags a, TextureViewFlags b)
{
	return static_cast<TextureViewFlags>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}

inline TextureViewFlags operator&(TextureViewFlags a, TextureViewFlags b)
{
	return static_cast<TextureViewFlags>(static_cast<uint8_t>(a) & static_cast<uint8_t>(b));
}

template <class T>
void SafeRelease(T& ppT)
{
	if (ppT)
	{
		ppT->Release();
		ppT = nullptr;
	}
}

const std::string DXErrorToStr(HRESULT hr);
const std::string ConvertStr(D3D_FEATURE_LEVEL level);

struct ContextSubmissionResult final
{
	uint32_t frameId = 0;
	uint32_t submissionIndex = 0;
};

struct BufferCreationDesc final
{
	uint32_t          size{ 0 };
	uint32_t          stride{ 0 };
	BufferViewFlags   viewFlags{ BufferViewFlags::none };
	BufferAccessFlags accessFlags{ BufferAccessFlags::gpuOnly };
	bool              isRawAccess{ false };
};

struct TextureCreationDesc final
{
	TextureCreationDesc()
	{
		resourceDesc.Format             = DXGI_FORMAT_UNKNOWN;
		resourceDesc.Width              = 0;
		resourceDesc.Height             = 0;
		resourceDesc.Flags              = D3D12_RESOURCE_FLAG_NONE;
		resourceDesc.DepthOrArraySize   = 1;
		resourceDesc.MipLevels          = 1;
		resourceDesc.SampleDesc.Count   = 1;
		resourceDesc.SampleDesc.Quality = 0;
		resourceDesc.Dimension          = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		resourceDesc.Layout             = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		resourceDesc.Alignment          = 0;
	}

	D3D12_RESOURCE_DESC resourceDesc{};
	TextureViewFlags    viewFlags{ TextureViewFlags::none };
};

struct Descriptor final
{
	bool IsValid() const { return CPUHandle.ptr != 0; }
	bool IsReferencedByShader() const { return GPUHandle.ptr != 0; }

	D3D12_CPU_DESCRIPTOR_HANDLE CPUHandle{ 0 };
	D3D12_GPU_DESCRIPTOR_HANDLE GPUHandle{ 0 };
	uint32_t                    heapIndex{ 0 };
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

struct PipelineResourceBinding final
{
	uint32_t  bindingIndex{ 0 };
	Resource* resource{ nullptr };
};

inline bool SortPipelineBindings(PipelineResourceBinding a, PipelineResourceBinding b)
{
	return a.bindingIndex < b.bindingIndex;
}

class PipelineResourceSpace final
{
public:
	void SetCBV(BufferResource* resource);
	void SetSRV(const PipelineResourceBinding& binding);
	void SetUAV(const PipelineResourceBinding& binding);
	void Lock();

	const auto  GetCBV() const { return m_CBV; }
	const auto& GetUAVs() const { return m_UAVs; }
	const auto& GetSRVs() const { return m_SRVs; }

	bool IsLocked() const { return m_isLocked; }

private:
	uint32_t getIndexOfBindingIndex(const std::vector<PipelineResourceBinding>& bindings, uint32_t bindingIndex);

	// If a resource space needs more than one CBV, it is likely a design flaw, as you want to consolidate these as much as possible if they have the same update frequency (which is contained by a PipelineResourceSpace). Of course, you can freely change this to a vector like the others if you want.
	BufferResource*                      m_CBV{ nullptr };
	std::vector<PipelineResourceBinding> m_UAVs;
	std::vector<PipelineResourceBinding> m_SRVs;
	bool                                 m_isLocked{ false };
};

struct PipelineResourceLayout final
{
	std::array<PipelineResourceSpace*, NUM_RESOURCE_SPACES> spaces{ nullptr };
};

struct RenderTargetDesc final
{
	std::array<DXGI_FORMAT, D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT> renderTargetFormats{ DXGI_FORMAT_UNKNOWN };
	uint8_t     numRenderTargets{ 0 };
	DXGI_FORMAT depthStencilFormat{ DXGI_FORMAT_UNKNOWN };
};

struct ShaderCreationDesc final
{
	std::wstring shaderName;
	std::wstring entryPoint;
	ShaderType   type{ ShaderType::compute };
};

struct Shader final
{
	IDxcBlob* shaderBlob{ nullptr };
};

struct GraphicsPipelineDesc final
{
	Shader*                       vertexShader{ nullptr };
	Shader*                       pixelShader{ nullptr };
	D3D12_RASTERIZER_DESC         rasterDesc{};
	D3D12_BLEND_DESC              blendDesc{};
	D3D12_DEPTH_STENCIL_DESC      depthStencilDesc{};
	RenderTargetDesc              renderTargetDesc{};
	DXGI_SAMPLE_DESC              sampleDesc{};
	D3D12_PRIMITIVE_TOPOLOGY_TYPE topology{ D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE };
};

inline GraphicsPipelineDesc GetDefaultGraphicsPipelineDesc()
{
	GraphicsPipelineDesc desc;
	desc.rasterDesc.FillMode = D3D12_FILL_MODE_SOLID;
	desc.rasterDesc.CullMode = D3D12_CULL_MODE_BACK;
	desc.rasterDesc.FrontCounterClockwise = false;
	desc.rasterDesc.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
	desc.rasterDesc.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
	desc.rasterDesc.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
	desc.rasterDesc.DepthClipEnable = true;
	desc.rasterDesc.MultisampleEnable = false;
	desc.rasterDesc.AntialiasedLineEnable = false;
	desc.rasterDesc.ForcedSampleCount = 0;
	desc.rasterDesc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

	desc.blendDesc.AlphaToCoverageEnable = false;
	desc.blendDesc.IndependentBlendEnable = false;

	const D3D12_RENDER_TARGET_BLEND_DESC defaultRenderTargetBlendDesc =
	{
		false, false,
		D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
		D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
		D3D12_LOGIC_OP_NOOP,
		D3D12_COLOR_WRITE_ENABLE_ALL,
	};

	for (UINT i = 0; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
	{
		desc.blendDesc.RenderTarget[i] = defaultRenderTargetBlendDesc;
	}

	desc.depthStencilDesc.DepthEnable = false;
	desc.depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	desc.depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
	desc.depthStencilDesc.StencilEnable = false;
	desc.depthStencilDesc.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
	desc.depthStencilDesc.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;

	const D3D12_DEPTH_STENCILOP_DESC defaultStencilOp =
	{ D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_COMPARISON_FUNC_ALWAYS };

	desc.depthStencilDesc.FrontFace = defaultStencilOp;
	desc.depthStencilDesc.BackFace = defaultStencilOp;

	desc.sampleDesc.Count = 1;
	desc.sampleDesc.Quality = 0;

	return desc;
}

struct ComputePipelineDesc final
{
	Shader* computeShader{ nullptr };
};

struct PipelineResourceMapping final
{
	std::array<std::optional<uint32_t>, NUM_RESOURCE_SPACES> cbvMapping{};
	std::array<std::optional<uint32_t>, NUM_RESOURCE_SPACES> tableMapping{};
};

struct PipelineStateObject final
{
	ID3D12PipelineState*    pipeline{ nullptr };
	ID3D12RootSignature*    rootSignature{ nullptr };
	PipelineType            pipelineType{ PipelineType::graphics };
	PipelineResourceMapping pipelineResourceMapping;
};

struct PipelineInfo final
{
	PipelineStateObject*          pipeline{ nullptr };
	std::vector<TextureResource*> renderTargets;
	TextureResource*              depthStencilTarget{ nullptr };
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

struct EndOfFrameFences final
{
	uint64_t graphicsQueueFence = 0;
	uint64_t computeQueueFence = 0;
	uint64_t copyQueueFence = 0;
};

#endif // RENDER_D3D12