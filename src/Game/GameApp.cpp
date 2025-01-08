#include "stdafx.h"

void GameApp()
{
	EngineAppCreateInfo engineAppCreateInfo{};
	EngineApp engine;
	if (engine.Create(engineAppCreateInfo))
	{
		auto& rhi = engine.GetRenderSystem();

		while (!engine.IsShouldClose())
		{
			engine.BeginFrame();

			auto commandAllocator = gRHI.commandAllocators[gRHI.currentBackBufferIndex];
			auto backBuffer = gRHI.backBuffers[gRHI.currentBackBufferIndex];

			// Reset command list and allocator.
			commandAllocator->Reset();
			gRHI.commandList->Reset(commandAllocator.Get(), nullptr);

			// Clear the render target.
			{
				FLOAT clearColor[] = { 0.4f, 0.6f, 0.9f, 1.0f };

				const CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(backBuffer.Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
				gRHI.commandList->ResourceBarrier(1, &barrier);

				auto rtvDescriptor = gRHI.backBuffersDescriptor[gRHI.currentBackBufferIndex].CPUHandle;
				auto dsvDescriptor = gRHI.depthStencilDescriptor.CPUHandle;
				gRHI.commandList->OMSetRenderTargets(1, &rtvDescriptor, FALSE, &dsvDescriptor);
				gRHI.commandList->ClearRenderTargetView(rtvDescriptor, clearColor, 0, nullptr);
				gRHI.commandList->ClearDepthStencilView(dsvDescriptor, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

				// Set the viewport and scissor rect.
				const D3D12_VIEWPORT viewport = { 0.0f, 0.0f, static_cast<float>(gRHI.frameBufferWidth), static_cast<float>(gRHI.frameBufferHeight), D3D12_MIN_DEPTH, D3D12_MAX_DEPTH };
				const D3D12_RECT scissorRect = { 0, 0, static_cast<LONG>(gRHI.frameBufferWidth), static_cast<LONG>(gRHI.frameBufferHeight) };
				gRHI.commandList->RSSetViewports(1, &viewport);
				gRHI.commandList->RSSetScissorRects(1, &scissorRect);
			}

			gRHI.Present();
			engine.EndFrame();
		}
	}
	engine.Destroy();
}