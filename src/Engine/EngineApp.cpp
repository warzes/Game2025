#include "stdafx.h"
#include "EngineApp.h"
#include "Log.h"
#include "WindowData.h"
//=============================================================================
bool RequestExitStatus = false;
void RequestExit()
{
	RequestExitStatus = true;
}
bool IsRequestExit()
{
	return RequestExitStatus;
}
//=============================================================================
bool EngineApp::Create(const EngineAppCreateInfo& createInfo)
{
	RequestExitStatus = false;

#if PLATFORM_WINDOWS && RENDER_D3D12
	if (FAILED(CoInitializeEx(nullptr, COINIT_MULTITHREADED)))
	{
		Fatal("Failed to call CoInitializeEx");
		return false;
	}
#endif // PLATFORM_WINDOWS

#if defined(_DEBUG) && PLATFORM_WINDOWS
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	if (!m_log.Create(createInfo.log)) return false;
	if (!m_window.Create(createInfo.window)) return false;
	m_window.ConnectInputSystem(&m_input);
	if (!m_input.Create(createInfo.input)) return false;

	WindowData windowData;
#if PLATFORM_WINDOWS
	windowData.hwnd = m_window.GetHWND();
	windowData.handleInstance = m_window.GetWindowInstance();
#endif // PLATFORM_WINDOWS
	windowData.width = m_window.GetWidth();
	windowData.height = m_window.GetHeight();

	if (!m_render.Create(windowData, createInfo.render)) return false;

	return !RequestExitStatus;
}
//=============================================================================
void EngineApp::Destroy()
{
	m_render.Destroy();
	m_input.Destroy();
	m_window.Destroy();
	m_log.Destroy();
	RequestExitStatus = true;
}
//=============================================================================
bool EngineApp::IsShouldClose() const
{
	return RequestExitStatus;
}
//=============================================================================
void EngineApp::BeginFrame()
{
	m_window.PollEvent();
	if (m_window.IsShouldClose())
	{
		// TODO: здесь можно обработать закрытие окна
		RequestExit();
		return;
	}
	m_input.Update();

	m_render.Resize(m_window.GetWidth(), m_window.GetHeight());
	m_render.BeginFrame();
}
//=============================================================================
void EngineApp::EndFrame()
{
	if (IsShouldClose()) return;
	m_render.EndFrame();
	m_render.Present();
}
//=============================================================================