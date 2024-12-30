#pragma once

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

			TextureResource& backBuffer = gRHI.GetCurrentBackBuffer();

			gRHI.graphicsContext->Reset();

			gRHI.graphicsContext->AddBarrier(backBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET);
			gRHI.graphicsContext->FlushBarriers();// если нужно несколько transition ресурсов, то их нужно вызвать до Flush

			gRHI.graphicsContext->ClearRenderTarget(backBuffer, glm::vec4(0.3f, 0.3f, 0.8f, 1.0f));

			gRHI.graphicsContext->AddBarrier(backBuffer, D3D12_RESOURCE_STATE_PRESENT);
			gRHI.graphicsContext->FlushBarriers();

			SubmitContextWork(*gRHI.graphicsContext);

			engine.EndFrame();
		}
	}
	engine.Destroy();
}