#pragma once

#include "RenderCore.h"
#include "CommandContextD3D12.h" // TODO: temp
#include "RHIBackend.h" // TODO: temp

struct WindowData;

class RenderSystem final
{
public:
	~RenderSystem();

	[[nodiscard]] bool Create(const WindowData& wndData, const RenderSystemCreateInfo& createInfo);
	void Destroy();

	void Resize(uint32_t width, uint32_t height);
	void BeginFrame();
	void EndFrame();
	void Present();

	glm::ivec2 GetFrameBufferSize() const;
};