#pragma once

#include "RenderCoreD3D12.h" // TODO: RenderCore.h

struct WindowData;

namespace RHIBackend
{
	[[nodiscard]] bool CreateAPI(const WindowData& wndData, const RenderSystemCreateInfo& createInfo);
	void DestroyAPI();

	void ResizeFrameBuffer(uint32_t width, uint32_t height);
	void BeginFrame();
	void EndFrame();
	void Present();
};