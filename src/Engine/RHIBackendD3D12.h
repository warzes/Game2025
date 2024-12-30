#pragma once

#if RENDER_D3D12

#include "RHICoreD3D12.h"

struct WindowData;
struct RenderSystemCreateInfo;

class RHIBackend final
{
public:
	~RHIBackend();

	[[nodiscard]] bool CreateAPI(const WindowData& wndData, const RenderSystemCreateInfo& createInfo);
	void DestroyAPI();

	void ResizeFrameBuffer(uint32_t width, uint32_t height);
	void BeginFrame();
	void EndFrame();

	void WaitForGpu();
	void MoveToNextFrame();

	RenderFeatures                  supportFeatures{};
	ComPtr<IDXGIAdapter4>           adapter;
	ComPtr<ID3D12Device14>          device;
	ComPtr<D3D12MA::Allocator>      allocator;

	// Synchronization objects
	ComPtr<ID3D12Fence>             fence;
	uint64_t                        fenceValues[NUM_BACK_BUFFERS] = {};
	Microsoft::WRL::Wrappers::Event fenceEvent;

	ComPtr<IDXGISwapChain4>         swapChain;
	ComPtr<ID3D12Resource>          backBuffers[NUM_BACK_BUFFERS];
	ComPtr<ID3D12Resource>          depthStencil;
	ComPtr<ID3D12DescriptorHeap>    rtvDescriptorHeap;
	ComPtr<ID3D12DescriptorHeap>    dsvDescriptorHeap;
	UINT                            rtvDescriptorSize; // размер одного дескриптора
	UINT                            currentBackBufferIndex{ 0 };
	uint32_t                        frameBufferWidth{ 0 };
	uint32_t                        frameBufferHeight{ 0 };
	const DXGI_FORMAT               backBufferFormat{ DXGI_FORMAT_R8G8B8A8_UNORM };
	const DXGI_FORMAT               depthBufferFormat{ DXGI_FORMAT_D32_FLOAT };

	bool                            vsync{ false };

	// TEMP =====================================
	ComPtr<ID3D12CommandQueue>        g_CommandQueue;
	ComPtr<ID3D12GraphicsCommandList> g_CommandList;
	ComPtr<ID3D12CommandAllocator>    g_CommandAllocators[NUM_BACK_BUFFERS];

	// TEMP =====================================

private:
	void enableDebugLayer(const RenderSystemCreateInfo& createInfo);
	bool createAdapter(const RenderSystemCreateInfo& createInfo);
	bool createDevice();
	void configInfoQueue();
	bool createAllocator();
};

extern RHIBackend gRHI;

#endif // RENDER_D3D12