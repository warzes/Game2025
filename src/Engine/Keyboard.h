#pragma once

#include "InputCore.h"

class KeyboardStateTracker final
{
public:
	KeyboardStateTracker() noexcept { Reset(); }

	void Update(const KeyState& state) noexcept;

	void Reset() noexcept;

	bool IsKeyPressed(Key key) const noexcept { return pressed.IsKeyDown(key); }
	bool IsKeyReleased(Key key) const noexcept { return released.IsKeyDown(key); }

	KeyState GetLastState() const noexcept { return lastState; }

	KeyState released;
	KeyState pressed;
	KeyState lastState;
};

class Keyboard final
{
	friend class InputSystem;
public:
	[[nodiscard]] bool Create();
	void Destroy();

	void Reset();

	const KeyState& GetState() const { return m_state; }

private:
#if PLATFORM_WINDOWS
	void processMessage(UINT message, WPARAM wParam, LPARAM lParam);
#endif

	KeyState m_state{};
};