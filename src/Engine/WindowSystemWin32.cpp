#include "stdafx.h"
#if PLATFORM_WINDOWS
#include "WindowSystem.h"
#include "InputSystem.h"
#include "Log.h"
//=============================================================================
namespace
{
	constexpr const auto windowClassName = L"Sapphire Window Class";
}
//=============================================================================
LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) noexcept
{
	LONG_PTR windowAddress = GetWindowLongPtr(hwnd, GWLP_USERDATA);
	WindowSystem* thisWindow = reinterpret_cast<WindowSystem*>(windowAddress);

	if (message == WM_DESTROY)
	{
		PostQuitMessage(0);
		return 0;
	}

	if (thisWindow)
	{
		switch (message)
		{
		case WM_DISPLAYCHANGE:
			thisWindow->displayChange();
			thisWindow->windowSizeChanged(static_cast<uint32_t>(LOWORD(lParam)), static_cast<uint32_t>(HIWORD(lParam)));
			break;
		case WM_ENTERSIZEMOVE:
			thisWindow->m_inSizemove = true;
			break;
		case WM_EXITSIZEMOVE:
			thisWindow->m_inSizemove = false;
			{
				RECT rc;
				GetClientRect(hwnd, &rc);
				thisWindow->windowSizeChanged(rc.right - rc.left, rc.bottom - rc.top);// TODO: возможно ненужно
			}
			break;
		case WM_SIZE:
			if (wParam == SIZE_MINIMIZED)
			{
				if (!thisWindow->m_minimized)
				{
					thisWindow->m_minimized = true;
					if (!thisWindow->m_inSuspend) thisWindow->suspending();
					thisWindow->m_inSuspend = true;
				}
			}
			else if (thisWindow->m_minimized)
			{
				thisWindow->m_minimized = false;
				if (thisWindow->m_inSuspend) thisWindow->resuming();
				thisWindow->m_inSuspend = false;
			}
			else if (!thisWindow->m_inSizemove)
			{
				thisWindow->windowSizeChanged(static_cast<uint32_t>(LOWORD(lParam)), static_cast<uint32_t>(HIWORD(lParam)));
			}
			break;

		case WM_ACTIVATEAPP:
			if (wParam) thisWindow->activated();
			else        thisWindow->deactivated();
			break;

		case WM_MOUSEACTIVATE:
			// Когда вы нажимаете на кнопку активации окна, мы хотим, чтобы мышь игнорировала его.
			return MA_ACTIVATEANDEAT;

		case WM_MENUCHAR:
			// A menu is active and the user presses a key that does not correspond to any mnemonic or accelerator key. Ignore so we don't produce an error beep.
			return MAKELRESULT(0, 1/*MNC_CLOSE*/);

		case WM_POWERBROADCAST:
			switch (wParam)
			{
			case PBT_APMQUERYSUSPEND:
				if (!thisWindow->m_inSuspend) thisWindow->suspending();
				thisWindow->m_inSuspend = true;
				return TRUE;

			case PBT_APMRESUMESUSPEND:
				if (!thisWindow->m_minimized)
				{
					if (thisWindow->m_inSuspend) thisWindow->resuming();
					thisWindow->m_inSuspend = false;
				}
				return TRUE;
			default:
				break;
			}
			break;

		case WM_SYSKEYDOWN:
			if (wParam == VK_RETURN && (lParam & 0x60000000) == 0x20000000)
			{
				// Implements the classic ALT+ENTER fullscreen toggle
				if (thisWindow->m_fullscreen)
				{
					SetWindowLongPtr(hwnd, GWL_STYLE, WS_OVERLAPPEDWINDOW);
					SetWindowLongPtr(hwnd, GWL_EXSTYLE, 0);

					ShowWindow(hwnd, SW_SHOWNORMAL);
					SetWindowPos(hwnd, HWND_TOP, 0, 0, thisWindow->m_width, thisWindow->m_height, SWP_NOMOVE | SWP_NOZORDER | SWP_FRAMECHANGED);
				}
				else
				{
					const int screenWidth = GetSystemMetrics(SM_CXSCREEN);
					const int screenHeight = GetSystemMetrics(SM_CYSCREEN);
					const int windowLeft = screenWidth / 2 - (int)thisWindow->m_width / 2;
					const int windowTop = screenHeight / 2 - (int)thisWindow->m_height / 2;

					SetWindowLongPtr(hwnd, GWL_STYLE, WS_POPUP);
					SetWindowLongPtr(hwnd, GWL_EXSTYLE, WS_EX_TOPMOST);
					SetWindowPos(hwnd, HWND_TOP, windowLeft, windowTop, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
					ShowWindow(hwnd, SW_SHOWMAXIMIZED);
				}

				thisWindow->m_fullscreen = !thisWindow->m_fullscreen;
			}
			break;
		default:
			break;
		}

		if (thisWindow->m_input)
			thisWindow->m_input->ProcessMessage(message, wParam, lParam);
	}

	return DefWindowProc(hwnd, message, wParam, lParam);
}
//=============================================================================
WindowSystem::~WindowSystem()
{
	assert(!m_hwnd);
	assert(!m_input);
}
//=============================================================================
bool WindowSystem::Create(const WindowSystemCreateInfo& createInfo)
{
	m_requestClose = true;
	m_handleInstance = GetModuleHandle(nullptr);
	m_fullscreen = createInfo.fullScreen;

	WNDCLASSEX windowClassInfo{ .cbSize = sizeof(WNDCLASSEX) };
	windowClassInfo.style         = CS_HREDRAW | CS_VREDRAW;
	windowClassInfo.lpfnWndProc   = WndProc;
	windowClassInfo.hInstance     = m_handleInstance;
	windowClassInfo.hIcon         = LoadIconW(nullptr, L"IDI_ICON");
	windowClassInfo.hIconSm       = LoadIconW(m_handleInstance, L"IDI_ICON");
	windowClassInfo.hCursor       = LoadCursor(nullptr, IDC_ARROW);
	windowClassInfo.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
	windowClassInfo.lpszClassName = windowClassName;
	if (!RegisterClassEx(&windowClassInfo))
	{
		Fatal("Window class registration failed!");
		return false;
	}

	const int screenWidth = GetSystemMetrics(SM_CXSCREEN);
	const int screenHeight = GetSystemMetrics(SM_CYSCREEN);
	const int windowLeft = screenWidth / 2 - (int)createInfo.width / 2;
	const int windowTop = screenHeight / 2 - (int)createInfo.height / 2;

	RECT rect = { 0, 0, static_cast<LONG>(createInfo.width), static_cast<LONG>(createInfo.height) };
	AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);
	LONG width = rect.right - rect.left;
	LONG height = rect.bottom - rect.top;

	m_hwnd = CreateWindowEx(0,
		windowClassName, L"Game",
		WS_OVERLAPPEDWINDOW,
		windowLeft, windowTop, width, height,
		nullptr, nullptr, m_handleInstance, nullptr);
	if (!m_hwnd)
	{
		Fatal("Window creation failed!");
		return false;
	}

	SetWindowLongPtr(m_hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));

	ShowWindow(m_hwnd, createInfo.maximize ? SW_SHOWMAXIMIZED : SW_SHOWNORMAL);

	GetClientRect(m_hwnd, &rect);
	m_width = static_cast<uint32_t>(rect.right - rect.left);
	m_height = static_cast<uint32_t>(rect.bottom - rect.top);

	m_inSizemove = false;
	m_inSuspend = false;
	m_minimized = false;
	m_requestClose = false;
	return true;
}
//=============================================================================
void WindowSystem::Destroy()
{
	m_input = nullptr;
	if (m_hwnd) DestroyWindow(m_hwnd);
	m_hwnd = nullptr;
	if (m_handleInstance) UnregisterClass(windowClassName, m_handleInstance);
	m_handleInstance = nullptr;
}
//=============================================================================
void WindowSystem::ConnectInputSystem(InputSystem* input)
{
	m_input = input;
	if (m_input) m_input->ConnectWindowSystem(this);
}
//=============================================================================
bool WindowSystem::IsShouldClose() const
{
	return m_requestClose;
}
//=============================================================================
void WindowSystem::PollEvent()
{
	while (PeekMessage(&m_msg, nullptr, 0, 0, PM_REMOVE))
	{
		TranslateMessage(&m_msg);
		DispatchMessage(&m_msg);

		if (m_msg.message == WM_QUIT)
		{
			m_requestClose = true;
			break;
		}
	}
}
//=============================================================================
uint32_t WindowSystem::GetPositionX() const
{
	RECT rect{};
	GetClientRect(m_hwnd, &rect);
	ClientToScreen(m_hwnd, (LPPOINT)&rect.left);
	ClientToScreen(m_hwnd, (LPPOINT)&rect.right);
	return rect.left;
}
//=============================================================================
uint32_t WindowSystem::GetPositionY() const
{
	RECT rect{};
	GetClientRect(m_hwnd, &rect);
	ClientToScreen(m_hwnd, (LPPOINT)&rect.left);
	ClientToScreen(m_hwnd, (LPPOINT)&rect.right);
	return rect.top;
}
//=============================================================================
void WindowSystem::displayChange()
{
	// TODO: dpi?
}
//=============================================================================
void WindowSystem::windowSizeChanged(uint32_t width, uint32_t height)
{
	m_width = width;
	m_height = height;
	// TODO: dpi?
}
//=============================================================================
void WindowSystem::suspending()
{
}
//=============================================================================
void WindowSystem::resuming()
{
	if (m_input) m_input->onResuming();
}
//=============================================================================
void WindowSystem::activated()
{
	if (m_input) m_input->onActivated();
}
//=============================================================================
void WindowSystem::deactivated()
{
}
//=============================================================================
#endif // PLATFORM_WINDOWS