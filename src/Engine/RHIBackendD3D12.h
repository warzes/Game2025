#pragma once

#if RENDER_D3D12

#include "RHICoreD3D12.h"
#include "ContextD3D12.h"
#include "DescriptorHeapD3D12.h"

struct WindowData;
struct RenderSystemCreateInfo;

class RHIBackend final
{
public:
	[[nodiscard]] bool CreateAPI(const WindowData& wndData, const RenderSystemCreateInfo& createInfo);
	void DestroyAPI();

	void ResizeFrameBuffer(uint32_t width, uint32_t height);
	void BeginFrame();
	void EndFrame();

	void Prepare(
		D3D12_RESOURCE_STATES beforeState = D3D12_RESOURCE_STATE_PRESENT,
		D3D12_RESOURCE_STATES afterState = D3D12_RESOURCE_STATE_RENDER_TARGET);
	void Present(D3D12_RESOURCE_STATES beforeState = D3D12_RESOURCE_STATE_RENDER_TARGET);

	void WaitForGpu();

	auto GetD3DDevice() const noexcept { return context.GetD3DDevice(); }
	auto GetCommandQueue() const noexcept { return commandQueue.Get(); }
	auto GetCommandList() const noexcept { return commandList.Get(); }
	auto GetCurrentCommandAllocator() const noexcept { return commandAllocators[currentBackBufferIndex].Get(); }
	auto GetSwapChain() const noexcept { return swapChain.Get(); }
	auto GetScreenViewport() const noexcept { return screenViewport; }
	auto GetScissorRect() const noexcept { return scissorRect; }
	auto GetCurrentFrameIndex() const noexcept { return currentBackBufferIndex; }

	auto GetBackBufferFormat() const noexcept { return backBufferFormat; }
	auto GetDepthBufferFormat() const noexcept { return depthBufferFormat; }

	auto GetRenderTargetView() const noexcept
	{
		return backBuffersDescriptor[currentBackBufferIndex].CPUHandle;
	}

	auto GetDepthStencilView() const noexcept
	{
		return depthStencilDescriptor.CPUHandle;
	}

	RenderFeatures                      supportFeatures{};
	ContextD3D12                        context;

	ComPtr<ID3D12CommandQueue>          commandQueue;
	ComPtr<ID3D12GraphicsCommandList10> commandList;
	ComPtr<ID3D12CommandAllocator>      commandAllocators[MAX_BACK_BUFFER_COUNT];

	// Synchronization objects
	ComPtr<ID3D12Fence>                 fence;
	uint64_t                            fenceValues[MAX_BACK_BUFFER_COUNT] = {};
	Microsoft::WRL::Wrappers::Event     fenceEvent;

	StagingDescriptorHeapD3D12*         RTVStagingDescriptorHeap{ nullptr };
	StagingDescriptorHeapD3D12*         DSVStagingDescriptorHeap{ nullptr };
	StagingDescriptorHeapD3D12*         CBVSRVUAVStagingDescriptorHeap{ nullptr };

	ComPtr<IDXGISwapChain4>             swapChain;
	ComPtr<ID3D12Resource>              backBuffers[MAX_BACK_BUFFER_COUNT];
	ComPtr<ID3D12Resource>              depthStencil;
	DescriptorD3D12                     backBuffersDescriptor[MAX_BACK_BUFFER_COUNT];
	DescriptorD3D12                     depthStencilDescriptor{};

	D3D12_VIEWPORT                      screenViewport{};
	D3D12_RECT                          scissorRect{};
	UINT                                currentBackBufferIndex{ 0 };
	uint32_t                            frameBufferWidth{ 0 };
	uint32_t                            frameBufferHeight{ 0 };
	const DXGI_FORMAT                   backBufferFormat{ DXGI_FORMAT_R8G8B8A8_UNORM }; // DXGI_FORMAT_B8G8R8A8_UNORM
	const DXGI_FORMAT                   depthBufferFormat{ DXGI_FORMAT_D32_FLOAT };

	bool                                vsync{ false };

private:
	bool setSize(uint32_t width, uint32_t height);
	bool createDescriptorHeap();
	bool createSwapChain(const WindowData& wndData);
	bool updateRenderTargetViews();
	void destroyRenderTargetViews();

	ComPtr<ID3D12CommandQueue> createCommandQueue(D3D12_COMMAND_LIST_TYPE type);

	void moveToNextFrame();
};

extern RHIBackend gRHI;

#endif // RENDER_D3D12