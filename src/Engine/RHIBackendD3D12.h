#pragma once

#if RENDER_D3D12

#include "RHICoreD3D12.h"
#include "ContextD3D12.h"
#include "DescriptorHeapD3D12.h"
#include "CommandQueueD3D12.h"
#include "FenceD3D12.h"
#include "SwapChainD3D12.h"

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
	auto GetCommandQueue() const noexcept { return commandQueue.Get().Get(); }
	auto GetCommandList() const noexcept { return commandList.Get(); }
	auto GetCurrentCommandAllocator() const noexcept { return commandAllocators[swapChain.GetCurrentBackBufferIndex()].Get(); }

	auto GetScreenViewport() const noexcept { return screenViewport; }
	auto GetScissorRect() const noexcept { return scissorRect; }
	auto GetCurrentFrameIndex() const noexcept { return swapChain.GetCurrentBackBufferIndex(); }

	auto GetBackBufferFormat() const noexcept { return swapChain.GetFormat(); }
	auto GetDepthBufferFormat() const noexcept { return depthBufferFormat; }

	auto GetRenderTargetView() const noexcept
	{
		return swapChain.GetRenderTargetView();
	}

	auto GetDepthStencilView() const noexcept
	{
		return depthStencilDescriptor.CPUHandle;
	}

	ContextD3D12                        context;
	SwapChainD3D12                      swapChain;

	CommandQueueD3D12                   commandQueue;
	ComPtr<ID3D12GraphicsCommandList10> commandList;
	ComPtr<ID3D12CommandAllocator>      commandAllocators[MAX_BACK_BUFFER_COUNT];

	StagingDescriptorHeapD3D12*         RTVStagingDescriptorHeap{ nullptr };
	StagingDescriptorHeapD3D12*         DSVStagingDescriptorHeap{ nullptr };
	StagingDescriptorHeapD3D12*         CBVSRVUAVStagingDescriptorHeap{ nullptr };

	ComPtr<ID3D12Resource>              depthStencil;
	DescriptorD3D12                     depthStencilDescriptor{};

	D3D12_VIEWPORT                      screenViewport{};
	D3D12_RECT                          scissorRect{};
	uint32_t                            frameBufferWidth{ 0 };
	uint32_t                            frameBufferHeight{ 0 };
	const DXGI_FORMAT                   depthBufferFormat{ DXGI_FORMAT_D32_FLOAT };

private:
	void resetVal();
	bool setSize(uint32_t width, uint32_t height);
	bool createDescriptorHeap();
	bool updateRenderTargetViews();
	void destroyRenderTargetViews();

	void moveToNextFrame();
};

extern RHIBackend gRHI;

#endif // RENDER_D3D12