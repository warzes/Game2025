#include "stdafx.h"
#if PLATFORM_WINDOWS
#include "InputSystem.h"
#include "WindowSystem.h"
//=============================================================================
InputSystem::~InputSystem()
{
	assert(!m_window);
}
//=============================================================================
bool InputSystem::Create(const InputSystemCreateInfo& createInfo)
{
	if (!m_keyboard.Create()) return false;
	m_keys.Reset();

	assert(m_window);
	if (!m_mouse.Create(m_window->GetHWND())) return false;

	return true;
}
//=============================================================================
void InputSystem::Destroy()
{
	m_mouse.Destroy();
	m_keyboard.Destroy();
	m_window = nullptr;
}
//=============================================================================
void InputSystem::ConnectWindowSystem(WindowSystem* window)
{
	m_window = window;
}
//=============================================================================
void InputSystem::Update()
{
	m_lastKeyboardState = m_keyboard.GetState();
	m_keys.Update(m_lastKeyboardState);
	m_lastBufferedMouseState = m_lastMouseState;
	m_lastMouseState = m_mouse.GetState(); // TODO: тут делается копия стейта, надо переделать
	m_mouseButtons.Update(m_lastMouseState);
}
//=============================================================================
void InputSystem::ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
	m_keyboard.processMessage(message, wParam, lParam);
	m_mouse.processMessage(message, wParam, lParam);
}
//=============================================================================
void InputSystem::SetMouseVisible(bool visible)
{
	m_mouse.SetVisible(visible);
}
//=============================================================================
glm::vec2 InputSystem::GetMousePosition() const
{
	return
	{
		static_cast<float>(m_lastMouseState.x),
		static_cast<float>(m_lastMouseState.y)
	};
}
//=============================================================================
glm::vec2 InputSystem::GetDeltaMouse() const
{
	return
	{
		static_cast<float>(m_lastMouseState.x) - static_cast<float>(m_lastBufferedMouseState.x),
		static_cast<float>(m_lastMouseState.y) - static_cast<float>(m_lastBufferedMouseState.y),
	};
}
//=============================================================================
bool InputSystem::IsPress(Key key) const
{
	return m_lastKeyboardState.IsKeyDown(key);
}
//=============================================================================
bool InputSystem::IsPress(MouseButton mouseKey) const
{
	switch (mouseKey)
	{
	case MouseButton::Left:     return m_lastMouseState.leftButton;
	case MouseButton::Middle:   return m_lastMouseState.middleButton;
	case MouseButton::Right:    return m_lastMouseState.rightButton;
	case MouseButton::XButton1: return m_lastMouseState.xButton1;
	case MouseButton::XButton2: return m_lastMouseState.xButton2;
	}

	return false;
}
//=============================================================================
void InputSystem::onResuming()
{
	m_keys.Reset();
	m_mouseButtons.Reset();
}
//=============================================================================
void InputSystem::onActivated()
{
	m_keys.Reset();
	m_mouseButtons.Reset();
}
//=============================================================================
#endif // PLATFORM_WINDOWS