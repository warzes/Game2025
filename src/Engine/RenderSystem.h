#pragma once

#include "RenderCore.h"

struct WindowData;

class RenderSystem final
{
public:
	~RenderSystem();

	[[nodiscard]] bool Create(const WindowData& wndData, const RenderSystemCreateInfo& createInfo);
	void Destroy();

	void Resize(uint32_t width, uint32_t height);
	void Present();

private:

};