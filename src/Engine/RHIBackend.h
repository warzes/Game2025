﻿#pragma once

#include "RenderCoreD3D12.h" // TODO: RenderCore.h

struct WindowData;

namespace RHIBackend
{
	[[nodiscard]] bool CreateAPI(const WindowData& wndData, const RenderSystemCreateInfo& createInfo);
	void DestroyAPI();

	void ResizeFrameBuffer(uint32_t width, uint32_t height);
	void Present();

	std::unique_ptr<BufferResource> CreateBuffer(const BufferCreationDesc& desc);

	void CopyDescriptorsSimple(uint32_t numDescriptors, D3D12_CPU_DESCRIPTOR_HANDLE destDescriptorRangeStart, D3D12_CPU_DESCRIPTOR_HANDLE srcDescriptorRangeStart, D3D12_DESCRIPTOR_HEAP_TYPE descriptorType);
};