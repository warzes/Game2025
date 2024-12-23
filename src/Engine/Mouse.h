#pragma once

#include "InputCore.h"

#if PLATFORM_WINDOWS
struct HandleCloser { void operator()(HANDLE h) noexcept { if (h) CloseHandle(h); } };
using ScopedHandle = std::unique_ptr<void, HandleCloser>;
#endif // PLATFORM_WINDOWS

class MouseButtonStateTracker final
{
public:
	MouseButtonStateTracker() noexcept { Reset(); }

	enum ButtonState : uint32_t
	{
		UP = 0,         // Button is up
		HELD = 1,       // Button is held down
		RELEASED = 2,   // Button was just released
		PRESSED = 3,    // Buton was just pressed
	};

	ButtonState leftButton;
	ButtonState middleButton;
	ButtonState rightButton;
	ButtonState xButton1;
	ButtonState xButton2;

	void Update(const MouseState& state) noexcept;

	void Reset() noexcept;

	MouseState GetLastState() const noexcept { return m_lastState; }

private:
	MouseState m_lastState;
};

class Mouse final
{
	friend class InputSystem;
public:
#if PLATFORM_WINDOWS
	[[nodiscard]] bool Create(HWND window);
#endif
	void Destroy();

	void Reset();

	MouseState GetState() const;
	void ResetScrollWheelValue();
	void SetMode(MouseMode mode);
	void EndOfInputFrame();
	bool IsConnected() const;
	bool IsVisible() const noexcept;
	void SetVisible(bool visible);

private:
#if PLATFORM_WINDOWS
	void processMessage(UINT message, WPARAM wParam, LPARAM lParam);
#endif

	void getState(MouseState& state) const;
	void clipToWindow() noexcept;

	mutable MouseState m_state{};
#if PLATFORM_WINDOWS
	HWND         m_window{ nullptr };
#endif
	MouseMode    m_mode{ MouseMode::Absolute };

	ScopedHandle m_scrollWheelValue;
	ScopedHandle m_relativeRead;
	ScopedHandle m_absoluteMode;
	ScopedHandle m_relativeMode;

	int          m_lastX{ 0 };
	int          m_lastY{ 0 };
	int          m_relativeX{ INT32_MAX };
	int          m_relativeY{ INT32_MAX };

	bool         m_inFocus{ true };
	bool         m_autoReset{ true };
};