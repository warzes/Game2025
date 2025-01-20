#include "stdafx.h"

упростить по https ://github.com/sawickiap/RegEngine

namespace testXXX
{

}

void ExampleRenderXXX()
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
				//gRHI.Prepare();
				gRHI.BeginDraw();

				//gRHI.ClearFrameBuffer(clearColor);

				PIXBeginEvent(commandList, PIX_COLOR_DEFAULT, L"Render");
				{
					// TODO: Add your rendering code here.
				}
				PIXEndEvent(commandList);

				gRHI.EndDraw();
				//gRHI.Present();
			}

			engine.EndFrame();
		}
	}
	engine.Destroy();
}