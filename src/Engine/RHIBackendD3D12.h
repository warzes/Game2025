#pragma once

#if RENDER_D3D12

#include "RenderCoreD3D12.h"
#include "ContextD3D12.h"
#include "RHIBackendD3D12.h"
#include "QueueD3D12.h"
#include "DescriptorHeapD3D12.h"

struct WindowData;

struct DestructionQueue final
{
	std::vector<std::unique_ptr<BufferResource>>      buffersToDestroy;
	std::vector<std::unique_ptr<TextureResource>>     texturesToDestroy;
	std::vector<std::unique_ptr<PipelineStateObject>> pipelinesToDestroy;
	std::vector<std::unique_ptr<Context>>             contextsToDestroy;
};

class RHIBackend final
{
public:
	~RHIBackend();

	[[nodiscard]] bool CreateAPI(const WindowData& wndData, const RenderSystemCreateInfo& createInfo);
	void DestroyAPI();

	void ResizeFrameBuffer(uint32_t width, uint32_t height);
	void BeginFrame();
	void EndFrame();
	void Present();

	uint32_t GetFrameId() const { return frameId; }
	RenderPassDescriptorHeap& GetSamplerHeap() { return *SamplerRenderPassDescriptorHeap; }
	RenderPassDescriptorHeap& GetSRVHeap(uint32_t frameIndex) { return *SRVRenderPassDescriptorHeaps[frameIndex]; }

	TextureResource& GetCurrentBackBuffer();

	Descriptor& GetImguiDescriptor(uint32_t index) { return ImguiDescriptors[index]; }
	UploadContext& GetUploadContextForCurrentFrame() { return *uploadContexts[frameId]; }

	IDXGIFactory7* DXGIFactory{ nullptr };
	IDXGIAdapter4* adapter{ nullptr };
	ID3D12Device14* device{ nullptr };
	D3D12MA::Allocator* allocator{ nullptr };

	QueueD3D12* graphicsQueue{ nullptr };
	QueueD3D12* computeQueue{ nullptr };
	QueueD3D12* copyQueue{ nullptr };

	StagingDescriptorHeap* RTVStagingDescriptorHeap{ nullptr };
	StagingDescriptorHeap* DSVStagingDescriptorHeap{ nullptr };
	StagingDescriptorHeap* SRVStagingDescriptorHeap{ nullptr };
	std::array<Descriptor, NUM_FRAMES_IN_FLIGHT> ImguiDescriptors;
	std::vector<uint32_t>                        FreeReservedDescriptorIndices;
	RenderPassDescriptorHeap* SamplerRenderPassDescriptorHeap{ nullptr };
	std::array<RenderPassDescriptorHeap*, NUM_FRAMES_IN_FLIGHT> SRVRenderPassDescriptorHeaps = { nullptr };

	IDXGISwapChain4* swapChain{ nullptr };
	std::array<TextureResource*, NUM_BACK_BUFFERS> backBuffers;

	uint32_t            frameBufferWidth{ 0 };
	uint32_t            frameBufferHeight{ 0 };

	uint32_t            frameId{ 0 };

	std::array<EndOfFrameFences, NUM_FRAMES_IN_FLIGHT> endOfFrameFences;

	std::array<UploadContext*, NUM_FRAMES_IN_FLIGHT> uploadContexts;
	std::array<std::vector<std::pair<uint64_t, D3D12_COMMAND_LIST_TYPE>>, NUM_FRAMES_IN_FLIGHT> contextSubmissions;
	std::array<DestructionQueue, NUM_FRAMES_IN_FLIGHT> destructionQueues;

private:
	void release();
};

extern RHIBackend gRHI;

// TODO: рассортировать

std::unique_ptr<BufferResource>      CreateBuffer(const BufferCreationDesc& desc);
std::unique_ptr<TextureResource>     CreateTexture(const TextureCreationDesc& desc);
std::unique_ptr<TextureResource>     CreateTextureFromFile(const std::string& texturePath);
std::unique_ptr<Shader>              CreateShader(const ShaderCreationDesc& desc);
std::unique_ptr<PipelineStateObject> CreateGraphicsPipeline(const GraphicsPipelineDesc& desc, const PipelineResourceLayout& layout);
std::unique_ptr<PipelineStateObject> CreateComputePipeline(const ComputePipelineDesc& desc, const PipelineResourceLayout& layout);
std::unique_ptr<GraphicsContext>     CreateGraphicsContext();
std::unique_ptr<ComputeContext>      CreateComputeContext();

void DestroyBuffer(std::unique_ptr<BufferResource> buffer);
void DestroyTexture(std::unique_ptr<TextureResource> texture);
void DestroyShader(std::unique_ptr<Shader> shader);
void DestroyPipelineStateObject(std::unique_ptr<PipelineStateObject> pso);
void DestroyContext(std::unique_ptr<Context> context);

ContextSubmissionResult SubmitContextWork(Context& context);
void WaitOnContextWork(ContextSubmissionResult submission, ContextWaitType waitType);
void WaitForIdle();

void CopyDescriptorsSimple(uint32_t numDescriptors, D3D12_CPU_DESCRIPTOR_HANDLE destDescriptorRangeStart, D3D12_CPU_DESCRIPTOR_HANDLE srcDescriptorRangeStart, D3D12_DESCRIPTOR_HEAP_TYPE descriptorType);
void CopyDescriptors(uint32_t numDestDescriptorRanges, const D3D12_CPU_DESCRIPTOR_HANDLE* destDescriptorRangeStarts, const uint32_t* destDescriptorRangeSizes,
	uint32_t numSrcDescriptorRanges, const D3D12_CPU_DESCRIPTOR_HANDLE* srcDescriptorRangeStarts, const uint32_t* srcDescriptorRangeSizes, D3D12_DESCRIPTOR_HEAP_TYPE descriptorType);

void ProcessDestructions(uint32_t frameIndex);

#endif // RENDER_D3D12