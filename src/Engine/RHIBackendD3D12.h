﻿#pragma once

#if RENDER_D3D12

#include "RHICoreD3D12.h"
#include "ContextD3D12.h"
#include "DescriptorHeapManagerD3D12.h"
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

	void ClearFrameBuffer(const glm::vec4& color);

	auto GetD3DDevice() const noexcept { return context.GetDevice(); }
	auto GetD3DAllocator() const noexcept { return context.GetAllocator(); }
	
	auto GetCommandList() const noexcept { return commandList.Get(); }
	auto GetCurrentCommandAllocator() const noexcept { return commandAllocators[swapChain.GetCurrentBackBufferIndex()].Get(); }

	auto GetScreenViewport() const noexcept { return swapChain.GetScreenViewport(); }
	auto GetScissorRect() const noexcept { return swapChain.GetScissorRect(); }
	auto GetFrameBufferWidth() const noexcept { return swapChain.GetFrameBufferWidth(); }
	auto GetFrameBufferHeight() const noexcept { return swapChain.GetFrameBufferHeight(); }

	auto GetCurrentFrameIndex() const noexcept { return swapChain.GetCurrentBackBufferIndex(); }

	auto GetBackBufferFormat() const noexcept { return swapChain.GetFormat(); }
	auto GetDepthStencilFormat() const noexcept { return swapChain.GetDepthStencilFormat(); }

	auto GetRenderTargetView() const noexcept
	{
		return swapChain.GetRenderTargetView();
	}

	auto GetDepthStencilView() const noexcept
	{
		return swapChain.GetDepthStencilView();
	}

	ContextD3D12                        context;
	SwapChainD3D12                      swapChain;

	CommandQueueD3D12                   graphicsQueue;
	CommandQueueD3D12                   computeQueue;
	CommandQueueD3D12                   copyQueue;

	ComPtr<ID3D12CommandAllocator>      commandAllocators[MAX_BACK_BUFFER_COUNT];
	ComPtr<ID3D12GraphicsCommandList10> commandList;


	DescriptorHeapManagerD3D12          descriptorHeapManager;

	// === new test??? ===>
	void BeginDraw();
	void EndDraw();

private:
	bool createCommandSystem();
};

extern RHIBackend gRHI;

#endif // RENDER_D3D12