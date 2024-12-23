#pragma once

#include "LogSystem.h"
#include "WindowSystem.h"
#include "InputSystem.h"

struct EngineAppCreateInfo final
{
	LogSystemCreateInfo    log{};
	WindowSystemCreateInfo window{};
	InputSystemCreateInfo  input{};
};

class EngineApp final
{
public:
	[[nodiscard]] bool Create(const EngineAppCreateInfo& createInfo);
	void Destroy();

	[[nodiscard]] bool IsShouldClose() const;

	void BeginFrame();
	void EndFrame();

	[[nodiscard]] auto& GetLogSystem() { return m_log; }
	[[nodiscard]] auto& GetWindowSystem() { return m_window; }
	[[nodiscard]] auto& GetInputSystem() { return m_input; }

private:
	LogSystem                   m_log{};
	WindowSystem                m_window{};
	InputSystem                 m_input{};	
};