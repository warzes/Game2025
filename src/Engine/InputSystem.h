#pragma once

#include "Keyboard.h"
#include "Mouse.h"

class WindowSystem;

struct InputSystemCreateInfo final
{

};

class InputSystem final
{
	friend class WindowSystem;
public:
	~InputSystem();

	[[nodiscard]] bool Create(const InputSystemCreateInfo& createInfo);
	void Destroy();

	void ConnectWindowSystem(WindowSystem* window);

	void Update();

#if PLATFORM_WINDOWS
	void ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam);
#endif

	void SetMouseVisible(bool visible);
	glm::vec2 GetMousePosition() const;
	glm::vec2 GetDeltaMouse() const;
	bool IsPress(Key key) const;
	bool IsPress(MouseButton mouseKey) const;

private:
	void onResuming();
	void onActivated();

	WindowSystem* m_window{ nullptr };

	Keyboard                m_keyboard{};
	KeyState                m_lastKeyboardState{ 0 };
	KeyboardStateTracker    m_keys{};

	Mouse                   m_mouse{};
	MouseState              m_lastMouseState{ 0 };
	MouseState              m_lastBufferedMouseState{ 0 };
	MouseButtonStateTracker m_mouseButtons;
};