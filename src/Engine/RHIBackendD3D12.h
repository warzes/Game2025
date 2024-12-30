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

	RenderFeatures             supportFeatures{};
	ComPtr<IDXGIAdapter4>      adapter;
	ComPtr<ID3D12Device14>     device;
	ComPtr<D3D12MA::Allocator> allocator;

	ComPtr<IDXGISwapChain4>    swapChain;
	uint32_t                   frameBufferWidth{ 0 };
	uint32_t                   frameBufferHeight{ 0 };

	bool                       vsync{ false };

	// TEMP =====================================
	ComPtr<ID3D12CommandQueue>        g_CommandQueue;
	ComPtr<ID3D12Resource>            g_BackBuffers[NUM_BACK_BUFFERS];
	ComPtr<ID3D12GraphicsCommandList> g_CommandList;
	ComPtr<ID3D12CommandAllocator>    g_CommandAllocators[NUM_BACK_BUFFERS];
	ComPtr<ID3D12DescriptorHeap>      g_RTVDescriptorHeap;
	UINT                              g_RTVDescriptorSize; // размер одного дескриптора
	UINT                              g_CurrentBackBufferIndex;

	// Synchronization objects
	ComPtr<ID3D12Fence>               g_Fence;
	uint64_t                          g_FenceValue = 0;
	uint64_t                          g_FrameFenceValues[NUM_BACK_BUFFERS] = {};
	HANDLE                            g_FenceEvent;
	// TEMP =====================================

private:
	void enableDebugLayer();
	bool createAdapter(const RenderSystemCreateInfo& createInfo);
	bool createDevice();
	void configInfoQueue();
	bool createAllocator();
};

extern RHIBackend gRHI;

#endif // RENDER_D3D12