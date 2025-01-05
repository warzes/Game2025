#pragma once

#if RENDER_D3D12

#include "RHICoreD3D12.h"
#include "ContextD3D12.h"

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
	auto GetCurrentBackBuffer() const noexcept { return backBuffers[currentBackBufferIndex].Get(); }
	auto GetDepthStencil() const noexcept { return depthStencil.Get(); }
	auto GetScreenViewport() const noexcept { return screenViewport; }
	auto GetScissorRect() const noexcept { return scissorRect; }
	auto GetCurrentFrameIndex() const noexcept { return currentBackBufferIndex; }

	auto GetBackBufferFormat() const noexcept { return backBufferFormat; }
	auto GetDepthBufferFormat() const noexcept { return depthBufferFormat; }

	auto GetRenderTargetView() const noexcept
	{
		const auto cpuHandle = rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
		return CD3DX12_CPU_DESCRIPTOR_HANDLE(cpuHandle, static_cast<INT>(currentBackBufferIndex), rtvDescriptorSize);
	}

	auto GetDepthStencilView() const noexcept
	{
		const auto cpuHandle = dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
		return CD3DX12_CPU_DESCRIPTOR_HANDLE(cpuHandle);
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

	ComPtr<IDXGISwapChain4>             swapChain;
	ComPtr<ID3D12Resource>              backBuffers[MAX_BACK_BUFFER_COUNT];
	ComPtr<ID3D12Resource>              depthStencil;
	ComPtr<ID3D12DescriptorHeap>        rtvDescriptorHeap;
	ComPtr<ID3D12DescriptorHeap>        dsvDescriptorHeap;
	UINT                                rtvDescriptorSize{ 0 }; // размер одного дескриптора
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
	bool createSwapChain(const WindowData& wndData);
	bool updateRenderTargetViews();

	ComPtr<ID3D12CommandQueue> createCommandQueue(D3D12_COMMAND_LIST_TYPE type);
	ComPtr<ID3D12DescriptorHeap> createDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t numDescriptors);

	void moveToNextFrame();
};

extern RHIBackend gRHI;

#endif // RENDER_D3D12