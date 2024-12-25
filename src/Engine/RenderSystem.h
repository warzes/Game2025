#pragma once

#include "RenderCore.h"
#include "ContextD3D12.h" // TODO: temp
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

	auto GetGraphicsContext() { return m_graphicsContext.get(); }

private:
	std::unique_ptr<GraphicsContext> m_graphicsContext; // TODO: temp
};