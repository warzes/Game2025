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

	SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
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
	// TODO: time metric
	{
		static uint64_t frameCounter = 0;
		static double elapsedSeconds = 0.0;
		static std::chrono::high_resolution_clock clock;
		static auto t0 = clock.now();

		frameCounter++;
		auto t1 = clock.now();
		auto deltaTime = t1 - t0;
		t0 = t1;
		elapsedSeconds += deltaTime.count() * 1e-9;
		if (elapsedSeconds > 1.0)
		{
			char buffer[500];
			auto fps = frameCounter / elapsedSeconds;
			sprintf_s(buffer, 500, "FPS: %f\n", fps);
			Print(buffer);

			frameCounter = 0;
			elapsedSeconds = 0.0;
		}
	}
	
	m_window.PollEvent();
	if (m_window.IsShouldClose())
	{
		// TODO: здесь можно обработать закрытие окна
		RequestExit();
		return;
	}
	m_input.Update();

	m_render.Resize(m_window.GetWidth(), m_window.GetHeight());
}
//=============================================================================
void EngineApp::EndFrame()
{
	if (IsShouldClose()) return;
}
//=============================================================================