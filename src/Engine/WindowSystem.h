#pragma once

#include "WindowCore.h"

class InputSystem;

class WindowSystem final
{
public:
	~WindowSystem();

	[[nodiscard]] bool Create(const WindowSystemCreateInfo& createInfo);
	void Destroy();

	void ConnectInputSystem(InputSystem* input);

	[[nodiscard]] bool IsShouldClose() const;
	void PollEvent();

	uint32_t GetPositionX() const;
	uint32_t GetPositionY() const;

	auto GetWidth() const { assert(m_width); return m_width; }
	auto GetHeight() const { assert(m_height); return m_height; }

#if PLATFORM_WINDOWS
	auto GetHWND() const { assert(m_hwnd); return m_hwnd; }
	auto GetWindowInstance() const { assert(m_handleInstance); return m_handleInstance; }
#endif // PLATFORM_WINDOWS

private:
	void displayChange();
	void windowSizeChanged(uint32_t width, uint32_t height);
	void suspending();
	void resuming();
	void activated();
	void deactivated();

#if PLATFORM_WINDOWS
	friend LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM) noexcept;

	InputSystem* m_input{ nullptr };

	HINSTANCE    m_handleInstance{ nullptr };
	HWND         m_hwnd{ nullptr };
	MSG          m_msg{};
	uint32_t     m_width{ 1600 };
	uint32_t     m_height{ 900 };
	bool         m_minimized{ false };
	bool         m_inSizemove{ false };
	bool         m_inSuspend{ false };
	bool         m_fullscreen{ false };
	bool         m_requestClose{ true };
#endif // PLATFORM_WINDOWS
};