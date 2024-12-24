#pragma once

#if RENDER_D3D12

#include "QueueD3D12.h"
#include "DescriptorHeapD3D12.h"
#include "ContextD3D12.h"

class RenderContext final
{
public:
	~RenderContext();

	void Release();

	IDXGIFactory7*      DXGIFactory{ nullptr };
	IDXGIAdapter4*      adapter{ nullptr };
	ID3D12Device8*      device{ nullptr };
	D3D12MA::Allocator* allocator{ nullptr };

	QueueD3D12*         graphicsQueue{ nullptr };
	QueueD3D12*         computeQueue{ nullptr };
	QueueD3D12*         copyQueue{ nullptr };

	StagingDescriptorHeap*                       RTVStagingDescriptorHeap{ nullptr };
	StagingDescriptorHeap*                       DSVStagingDescriptorHeap{ nullptr };
	StagingDescriptorHeap*                       SRVStagingDescriptorHeap{ nullptr };
	std::array<Descriptor, NUM_FRAMES_IN_FLIGHT> ImguiDescriptors;
	std::vector<uint32_t>                        FreeReservedDescriptorIndices;
	RenderPassDescriptorHeap*                    SamplerRenderPassDescriptorHeap{ nullptr };
	std::array<RenderPassDescriptorHeap*, NUM_FRAMES_IN_FLIGHT> SRVRenderPassDescriptorHeaps = { nullptr };

	std::array<UploadContext*, NUM_FRAMES_IN_FLIGHT> uploadContexts;


	uint32_t            frameBufferWidth{ 0 };
	uint32_t            frameBufferHeight{ 0 };
};

extern RenderContext gRenderContext;

#endif // RENDER_D3D12