#include "stdafx.h"

void GameApp()
{
	EngineAppCreateInfo engineAppCreateInfo{};
	EngineApp engine;
	if (engine.Create(engineAppCreateInfo))
	{
		while (!engine.IsShouldClose())
		{
			engine.BeginFrame();


			engine.EndFrame();
		}
	}
	engine.Destroy();
}