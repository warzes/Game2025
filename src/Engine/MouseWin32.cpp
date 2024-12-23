#include "stdafx.h"
#if PLATFORM_WINDOWS
#include "Mouse.h"
#include "Log.h"
//=============================================================================
void  MouseButtonStateTracker::Update(const MouseState& state) noexcept
{
#define UPDATE_BUTTON_STATE(field) field = static_cast<ButtonState>( ( !!state.field ) | ( ( !!state.field ^ !!m_lastState.field ) << 1 ) )

	UPDATE_BUTTON_STATE(leftButton);

	assert((!state.leftButton && !m_lastState.leftButton) == (leftButton == UP));
	assert((state.leftButton && m_lastState.leftButton) == (leftButton == HELD));
	assert((!state.leftButton && m_lastState.leftButton) == (leftButton == RELEASED));
	assert((state.leftButton && !m_lastState.leftButton) == (leftButton == PRESSED));

	UPDATE_BUTTON_STATE(middleButton);
	UPDATE_BUTTON_STATE(rightButton);
	UPDATE_BUTTON_STATE(xButton1);
	UPDATE_BUTTON_STATE(xButton2);

	m_lastState = state;

#undef UPDATE_BUTTON_STATE
}
//=============================================================================
void MouseButtonStateTracker::Reset() noexcept
{
	memset(this, 0, sizeof(MouseButtonStateTracker));
}
//=============================================================================
bool Mouse::Create(HWND window)
{
	m_window = window;

	m_scrollWheelValue.reset(CreateEventEx(nullptr, nullptr, CREATE_EVENT_MANUAL_RESET, EVENT_MODIFY_STATE | SYNCHRONIZE));
	m_relativeRead.reset(CreateEventEx(nullptr, nullptr, CREATE_EVENT_MANUAL_RESET, EVENT_MODIFY_STATE | SYNCHRONIZE));
	m_absoluteMode.reset(CreateEventEx(nullptr, nullptr, 0, EVENT_MODIFY_STATE | SYNCHRONIZE));
	m_relativeMode.reset(CreateEventEx(nullptr, nullptr, 0, EVENT_MODIFY_STATE | SYNCHRONIZE));
	if (!m_scrollWheelValue || !m_relativeRead || !m_absoluteMode || !m_relativeMode)
	{
		Fatal("CreateEventEx");
		return false;
	}

	RAWINPUTDEVICE Rid;
	Rid.usUsagePage = 0x1 /* HID_USAGE_PAGE_GENERIC */;
	Rid.usUsage = 0x2 /* HID_USAGE_GENERIC_MOUSE */;
	Rid.dwFlags = RIDEV_INPUTSINK;
	Rid.hwndTarget = window;
	if (!RegisterRawInputDevices(&Rid, 1, sizeof(RAWINPUTDEVICE)))
	{
		Fatal("RegisterRawInputDevices");
		return false;
	}

	return true;
}
//=============================================================================
void Mouse::Destroy()
{
}
//=============================================================================
void Mouse::Reset()
{
}
//=============================================================================
MouseState Mouse::GetState() const
{
	MouseState state;
	getState(state);
	return state;
}
//=============================================================================
void Mouse::ResetScrollWheelValue()
{
	SetEvent(m_scrollWheelValue.get());
}
//=============================================================================
void Mouse::SetMode(MouseMode mode)
{
	if (m_mode == mode)
		return;

	SetEvent((mode == MouseMode::Absolute) ? m_absoluteMode.get() : m_relativeMode.get());

	// Send a WM_HOVER as a way to 'kick' the message processing even if the mouse is still.
	TRACKMOUSEEVENT tme;
	tme.cbSize = sizeof(tme);
	tme.dwFlags = TME_HOVER;
	tme.hwndTrack = m_window;
	tme.dwHoverTime = 1;
	if (!TrackMouseEvent(&tme))
	{
		Fatal("TrackMouseEvent");
	}
}
//=============================================================================
void Mouse::EndOfInputFrame()
{
	m_autoReset = false;

	if (m_mode == MouseMode::Relative)
	{
		m_state.x = m_state.y = 0;
	}
}
//=============================================================================
bool Mouse::IsConnected() const
{
	return GetSystemMetrics(SM_MOUSEPRESENT) != 0;
}
//=============================================================================
bool Mouse::IsVisible() const noexcept
{
	if (m_mode == MouseMode::Relative)
		return false;

	CURSORINFO info = { sizeof(CURSORINFO), 0, nullptr, {} };
	if (!GetCursorInfo(&info))
		return false;

	return (info.flags & CURSOR_SHOWING) != 0;
}
//=============================================================================
void Mouse::SetVisible(bool visible)
{
	if (m_mode == MouseMode::Relative)
		return;

	CURSORINFO info = { sizeof(CURSORINFO), 0, nullptr, {} };
	if (!GetCursorInfo(&info))
	{
		Fatal("GetCursorInfo");
	}

	const bool isVisible = (info.flags & CURSOR_SHOWING) != 0;
	if (isVisible != visible)
	{
		ShowCursor(visible);
	}
}
//=============================================================================
void Mouse::processMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
	// First handle any pending scroll wheel reset event.
	switch (WaitForSingleObjectEx(m_scrollWheelValue.get(), 0, FALSE))
	{
	default:
	case WAIT_TIMEOUT:
		break;

	case WAIT_OBJECT_0:
		m_state.scrollWheelValue = 0;
		ResetEvent(m_scrollWheelValue.get());
		break;

	case WAIT_FAILED:
		Fatal("WaitForMultipleObjectsEx");
		break;
	}

	// Next handle mode change events.
	HANDLE events[2] = { m_absoluteMode.get(), m_relativeMode.get() };
	switch (WaitForMultipleObjectsEx(static_cast<DWORD>(std::size(events)), events, FALSE, 0, FALSE))
	{
	case WAIT_TIMEOUT:
		break;

	case WAIT_OBJECT_0:
	{
		m_mode = MouseMode::Absolute;
		ClipCursor(nullptr);

		POINT point;
		point.x = m_lastX;
		point.y = m_lastY;

		// We show the cursor before moving it to support Remote Desktop
		ShowCursor(TRUE);

		if (MapWindowPoints(m_window, nullptr, &point, 1))
		{
			SetCursorPos(point.x, point.y);
		}
		m_state.x = m_lastX;
		m_state.y = m_lastY;
	}
	break;

	case (WAIT_OBJECT_0 + 1):
	{
		ResetEvent(m_relativeRead.get());

		m_mode = MouseMode::Relative;
		m_state.x = m_state.y = 0;
		m_relativeX = INT32_MAX;
		m_relativeY = INT32_MAX;

		ShowCursor(FALSE);

		clipToWindow();
	}
	break;

	case WAIT_FAILED:
		Fatal("WaitForMultipleObjectsEx");
		break;
	}

	switch (message)
	{
	case WM_ACTIVATE:
	case WM_ACTIVATEAPP:
		if (wParam)
		{
			m_inFocus = true;

			if (m_mode == MouseMode::Relative)
			{
				m_state.x = m_state.y = 0;

				ShowCursor(FALSE);

				clipToWindow();
			}
		}
		else
		{
			const int scrollWheel = m_state.scrollWheelValue;
			memset(&m_state, 0, sizeof(MouseState));
			m_state.scrollWheelValue = scrollWheel;

			if (m_mode == MouseMode::Relative)
			{
				ClipCursor(nullptr);
			}

			m_inFocus = false;
		}
		return;

	case WM_INPUT:
		if (m_inFocus && m_mode == MouseMode::Relative)
		{
			RAWINPUT raw;
			UINT rawSize = sizeof(raw);

			const UINT resultData = GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, &raw, &rawSize, sizeof(RAWINPUTHEADER));
			if (resultData == UINT(-1))
			{
				Fatal("GetRawInputData");
			}

			if (raw.header.dwType == RIM_TYPEMOUSE)
			{
				if (!(raw.data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE))
				{
					m_state.x += raw.data.mouse.lLastX;
					m_state.y += raw.data.mouse.lLastY;

					ResetEvent(m_relativeRead.get());
				}
				else if (raw.data.mouse.usFlags & MOUSE_VIRTUAL_DESKTOP)
				{
					// This is used to make Remote Desktop sessons work
					const int width = GetSystemMetrics(SM_CXVIRTUALSCREEN);
					const int height = GetSystemMetrics(SM_CYVIRTUALSCREEN);

					auto const x = static_cast<int>((float(raw.data.mouse.lLastX) / 65535.0f) * float(width));
					auto const y = static_cast<int>((float(raw.data.mouse.lLastY) / 65535.0f) * float(height));

					if (m_relativeX == INT32_MAX)
					{
						m_state.x = m_state.y = 0;
					}
					else
					{
						m_state.x = x - m_relativeX;
						m_state.y = y - m_relativeY;
					}

					m_relativeX = x;
					m_relativeY = y;

					ResetEvent(m_relativeRead.get());
				}
			}
		}
		return;

	case WM_MOUSEMOVE:
		break;

	case WM_LBUTTONDOWN:
		m_state.leftButton = true;
		break;

	case WM_LBUTTONUP:
		m_state.leftButton = false;
		break;

	case WM_RBUTTONDOWN:
		m_state.rightButton = true;
		break;

	case WM_RBUTTONUP:
		m_state.rightButton = false;
		break;

	case WM_MBUTTONDOWN:
		m_state.middleButton = true;
		break;

	case WM_MBUTTONUP:
		m_state.middleButton = false;
		break;

	case WM_MOUSEWHEEL:
		m_state.scrollWheelValue += GET_WHEEL_DELTA_WPARAM(wParam);
		return;

	case WM_XBUTTONDOWN:
		switch (GET_XBUTTON_WPARAM(wParam))
		{
		case XBUTTON1:
			m_state.xButton1 = true;
			break;

		case XBUTTON2:
			m_state.xButton2 = true;
			break;

		default:
			break;
		}
		break;

	case WM_XBUTTONUP:
		switch (GET_XBUTTON_WPARAM(wParam))
		{
		case XBUTTON1:
			m_state.xButton1 = false;
			break;

		case XBUTTON2:
			m_state.xButton2 = false;
			break;

		default:
			break;
		}
		break;

	case WM_MOUSEHOVER:
		break;

	default:
		// Not a mouse message, so exit
		return;
	}

	if (m_mode == MouseMode::Absolute)
	{
		// All mouse messages provide a new pointer position
		const int xPos = static_cast<short>(LOWORD(lParam)); // GET_X_LPARAM(lParam);
		const int yPos = static_cast<short>(HIWORD(lParam)); // GET_Y_LPARAM(lParam);

		m_state.x = m_lastX = xPos;
		m_state.y = m_lastY = yPos;
	}
}
//=============================================================================
void Mouse::getState(MouseState& state) const
{
	memcpy(&state, &m_state, sizeof(MouseState));
	state.positionMode = m_mode;

	DWORD result = WaitForSingleObjectEx(m_scrollWheelValue.get(), 0, FALSE);
	if (result == WAIT_FAILED)
		Fatal("WaitForSingleObjectEx");

	if (result == WAIT_OBJECT_0)
	{
		state.scrollWheelValue = 0;
	}

	if (state.positionMode == MouseMode::Relative)
	{
		result = WaitForSingleObjectEx(m_relativeRead.get(), 0, FALSE);

		if (result == WAIT_FAILED)
			Fatal("WaitForSingleObjectEx");

		if (result == WAIT_OBJECT_0)
		{
			state.x = state.y = 0;
		}
		else
		{
			SetEvent(m_relativeRead.get());
		}

		if (m_autoReset)
		{
			m_state.x = m_state.y = 0;
		}
	}
}
//=============================================================================
void Mouse::clipToWindow() noexcept
{
	RECT rect = {};
	std::ignore = GetClientRect(m_window, &rect);

	POINT ul;
	ul.x = rect.left;
	ul.y = rect.top;

	POINT lr;
	lr.x = rect.right;
	lr.y = rect.bottom;

	std::ignore = MapWindowPoints(m_window, nullptr, &ul, 1);
	std::ignore = MapWindowPoints(m_window, nullptr, &lr, 1);

	rect.left = ul.x;
	rect.top = ul.y;

	rect.right = lr.x;
	rect.bottom = lr.y;

	ClipCursor(&rect);
}
//=============================================================================
#endif // PLATFORM_WINDOWS