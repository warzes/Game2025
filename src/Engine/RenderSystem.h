#pragma once

#include "RenderCore.h"

#if RENDER_D3D12
#	include "RHIBackendD3D12.h" // TODO: temp
#endif // RENDER_D3D12

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

	glm::ivec2 GetFrameBufferSize() const;
};