#pragma once

#if RENDER_D3D12

#include "oRenderCoreD3D12.h"
#include "oCommandContextD3D12.h"
#include "oRHIBackendD3D12.h"
#include "oCommandQueueD3D12.h"
#include "oDescriptorHeapD3D12.h"

struct WindowData;

struct DestructionQueue final
{
	std::vector<std::unique_ptr<BufferResource>>      buffersToDestroy;
	std::vector<std::unique_ptr<TextureResource>>     texturesToDestroy;
	std::vector<std::unique_ptr<PipelineStateObject>> pipelinesToDestroy;
	std::vector<std::unique_ptr<CommandContextD3D12>> contextsToDestroy;
};

class oRHIBackend final
{
public:
	~oRHIBackend();

	[[nodiscard]] bool CreateAPI(const WindowData& wndData, const RenderSystemCreateInfo& createInfo);
	void DestroyAPI();

	void ResizeFrameBuffer(uint32_t width, uint32_t height);
	void BeginFrame();
	void EndFrame();
	void Present();

	uint32_t GetCurrentBackBufferIndex() const { return currentBackBufferIndex; }
	RenderPassDescriptorHeap& GetSamplerHeap() { return *samplerRenderPassDescriptorHeap; }
	RenderPassDescriptorHeap& GetSRVHeap(uint32_t frameIndex) { return *CBVSRVUAVRenderPassDescriptorHeaps[frameIndex]; }

	TextureResource& GetCurrentBackBuffer();

	Descriptor& GetImguiDescriptor(uint32_t index) { return ImguiDescriptors[index]; }
	UploadCommandContextD3D12& GetUploadContextForCurrentFrame() { return *uploadContexts[currentBackBufferIndex]; }

	RenderFeatures               supportFeatures{};
	ComPtr<IDXGIAdapter4>        adapter{ nullptr };
	ComPtr<ID3D12Device14>       device{ nullptr };
	ComPtr<D3D12MA::Allocator>   allocator{ nullptr };

	oCommandQueueD3D12*           graphicsQueue{ nullptr };
	oCommandQueueD3D12*           computeQueue{ nullptr };
	oCommandQueueD3D12*           copyQueue{ nullptr };

	StagingDescriptorHeap*       RTVStagingDescriptorHeap{ nullptr };
	StagingDescriptorHeap*       DSVStagingDescriptorHeap{ nullptr };
	StagingDescriptorHeap*       CBVSRVUAVStagingDescriptorHeap{ nullptr };
	RenderPassDescriptorHeap*    samplerRenderPassDescriptorHeap{ nullptr };
	RenderPassDescriptorHeap*    CBVSRVUAVRenderPassDescriptorHeaps[NUM_FRAMES_IN_FLIGHT]{};
	Descriptor                   ImguiDescriptors[NUM_FRAMES_IN_FLIGHT]{};

	ComPtr<IDXGISwapChain4>      swapChain;
	TextureResource*             backBuffers[MAX_BACK_BUFFER_COUNT]{};
	uint32_t                     frameBufferWidth{ 0 };
	uint32_t                     frameBufferHeight{ 0 };
	uint32_t                     currentBackBufferIndex{ 0 }; // TODO: юзать swapChain->GetCurrentBackBufferIndex()



	GraphicsCommandContextD3D12* graphicsContext{ nullptr };
	UploadCommandContextD3D12*   uploadContexts[NUM_FRAMES_IN_FLIGHT]{};

	std::vector<uint32_t>        freeReservedDescriptorIndices;

	std::array<EndOfFrameFences, NUM_FRAMES_IN_FLIGHT> endOfFrameFences;


	std::array<std::vector<std::pair<uint64_t, D3D12_COMMAND_LIST_TYPE>>, NUM_FRAMES_IN_FLIGHT> contextSubmissions;
	std::array<DestructionQueue, NUM_FRAMES_IN_FLIGHT> destructionQueues;

private:
	void enableDebugLayer();
	bool createAdapter();
	bool createDevice();
	void configInfoQueue();
	bool createAllocator();
	bool createCommandQueue();
	bool createDescriptorHeap();
	bool createSwapChain(const WindowData& wndData);
	bool createMainRenderTarget();
	void destroyMainRenderTarget();
	void release();
};

extern oRHIBackend ogRHI;

// TODO: рассортировать

std::unique_ptr<BufferResource>      CreateBuffer(const BufferCreationDesc& desc);
std::unique_ptr<TextureResource>     CreateTexture(const TextureCreationDesc& desc);
std::unique_ptr<TextureResource>     CreateTextureFromFile(const std::string& texturePath);
std::unique_ptr<Shader>              CreateShader(const ShaderCreationDesc& desc);
std::unique_ptr<PipelineStateObject> CreateGraphicsPipeline(const GraphicsPipelineDesc& desc, const PipelineResourceLayout& layout);
std::unique_ptr<PipelineStateObject> CreateComputePipeline(const ComputePipelineDesc& desc, const PipelineResourceLayout& layout);
std::unique_ptr<GraphicsCommandContextD3D12>     CreateGraphicsContext();
std::unique_ptr<ComputeCommandContextD3D12>      CreateComputeContext();

void DestroyBuffer(std::unique_ptr<BufferResource> buffer);
void DestroyTexture(std::unique_ptr<TextureResource> texture);
void DestroyShader(std::unique_ptr<Shader> shader);
void DestroyPipelineStateObject(std::unique_ptr<PipelineStateObject> pso);
void DestroyContext(std::unique_ptr<CommandContextD3D12> context);

ContextSubmissionResult SubmitContextWork(CommandContextD3D12& context);
void WaitOnContextWork(ContextSubmissionResult submission, ContextWaitType waitType);
void WaitForIdle();

void CopyDescriptorsSimple(uint32_t numDescriptors, D3D12_CPU_DESCRIPTOR_HANDLE destDescriptorRangeStart, D3D12_CPU_DESCRIPTOR_HANDLE srcDescriptorRangeStart, D3D12_DESCRIPTOR_HEAP_TYPE descriptorType);
void CopyDescriptors(uint32_t numDestDescriptorRanges, const D3D12_CPU_DESCRIPTOR_HANDLE* destDescriptorRangeStarts, const uint32_t* destDescriptorRangeSizes,
	uint32_t numSrcDescriptorRanges, const D3D12_CPU_DESCRIPTOR_HANDLE* srcDescriptorRangeStarts, const uint32_t* srcDescriptorRangeSizes, D3D12_DESCRIPTOR_HEAP_TYPE descriptorType);

void ProcessDestructions(uint32_t frameIndex);

#endif // RENDER_D3D12