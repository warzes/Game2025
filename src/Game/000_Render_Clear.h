#include "stdafx.h"

void ExampleRender000()
{
	EngineAppCreateInfo engineAppCreateInfo{};
	EngineApp engine;
	if (engine.Create(engineAppCreateInfo))
	{
		const glm::vec4 clearColor = { 0.4f, 0.6f, 0.9f, 1.0f };

		auto commandList = gRHI.GetCommandList();

		while (!engine.IsShouldClose())
		{
			engine.BeginFrame();

			// Update
			{
				PIXBeginEvent(PIX_COLOR_DEFAULT, L"Update");
				{
					// TODO: Add your logic here.
				}
				PIXEndEvent();
			}

			// Render
			{
				gRHI.Prepare();

				gRHI.ClearFrameBuffer(clearColor);

				PIXBeginEvent(commandList, PIX_COLOR_DEFAULT, L"Render");
				{
					// TODO: Add your rendering code here.
				}
				PIXEndEvent(commandList);
	
				gRHI.Present();
			}

			engine.EndFrame();
		}
	}
	engine.Destroy();
}