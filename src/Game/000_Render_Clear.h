#include "stdafx.h"

void ExampleRender000()
{
	EngineAppCreateInfo engineAppCreateInfo{};
	EngineApp engine;
	if (engine.Create(engineAppCreateInfo))
	{
		auto& rhi = engine.GetRenderSystem();

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

				// Clear the render target.
				{
					const FLOAT clearColor[] = { 0.4f, 0.6f, 0.9f, 1.0f };
					auto commandList = gRHI.GetCommandList();
					const auto rtvDescriptor = gRHI.GetRenderTargetView();
					const auto dsvDescriptor = gRHI.GetDepthStencilView();
					const auto viewport = gRHI.GetScreenViewport();
					const auto scissorRect = gRHI.GetScissorRect();

					PIXBeginEvent(commandList, PIX_COLOR_DEFAULT, L"Clear");

					commandList->OMSetRenderTargets(1, &rtvDescriptor, FALSE, &dsvDescriptor);
					commandList->ClearRenderTargetView(rtvDescriptor, clearColor, 0, nullptr);
					commandList->ClearDepthStencilView(dsvDescriptor, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
					commandList->RSSetViewports(1, &viewport);
					commandList->RSSetScissorRects(1, &scissorRect);

					PIXEndEvent(commandList);
				}

				auto commandList = gRHI.GetCommandList();
				PIXBeginEvent(commandList, PIX_COLOR_DEFAULT, L"Render");
				{
					// TODO: Add your rendering code here.
				}
				PIXEndEvent(commandList);

				PIXBeginEvent(PIX_COLOR_DEFAULT, L"Present");
				gRHI.Present();
				PIXEndEvent();
			}

			engine.EndFrame();
		}
	}
	engine.Destroy();
}