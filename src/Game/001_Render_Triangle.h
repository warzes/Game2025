#include "stdafx.h"
#include "Engine/TempD3D12.h"

void ExampleRender001()
{
	EngineAppCreateInfo engineAppCreateInfo{};
	EngineApp engine;
	if (engine.Create(engineAppCreateInfo))
	{
		auto& rhi = engine.GetRenderSystem();

		while (!engine.IsShouldClose())
		{
			engine.BeginFrame();

			auto commandAllocator = gRHI.g_CommandAllocators[gRHI.currentBackBufferIndex];
			auto backBuffer = gRHI.backBuffers[gRHI.currentBackBufferIndex];

			// Reset command list and allocator.
			commandAllocator->Reset();
			gRHI.g_CommandList->Reset(commandAllocator.Get(), nullptr);

			// Clear the render target.
			{
				FLOAT clearColor[] = { 0.4f, 0.6f, 0.9f, 1.0f };

				const CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(backBuffer.Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
				gRHI.g_CommandList->ResourceBarrier(1, &barrier);


				const auto cpuHandle = gRHI.rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
				const auto cpuHandleDSV = gRHI.dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
				const CD3DX12_CPU_DESCRIPTOR_HANDLE rtvDescriptor(cpuHandle, static_cast<INT>(gRHI.currentBackBufferIndex), gRHI.rtvDescriptorSize);
				CD3DX12_CPU_DESCRIPTOR_HANDLE dsvDescriptor(cpuHandleDSV);
				gRHI.g_CommandList->OMSetRenderTargets(1, &rtvDescriptor, FALSE, &dsvDescriptor);
				gRHI.g_CommandList->ClearRenderTargetView(rtvDescriptor, clearColor, 0, nullptr);
				gRHI.g_CommandList->ClearDepthStencilView(dsvDescriptor, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

				// Set the viewport and scissor rect.
				const D3D12_VIEWPORT viewport = { 0.0f, 0.0f, static_cast<float>(gRHI.frameBufferWidth), static_cast<float>(gRHI.frameBufferHeight), D3D12_MIN_DEPTH, D3D12_MAX_DEPTH };
				const D3D12_RECT scissorRect = { 0, 0, static_cast<LONG>(gRHI.frameBufferWidth), static_cast<LONG>(gRHI.frameBufferHeight) };
				gRHI.g_CommandList->RSSetViewports(1, &viewport);
				gRHI.g_CommandList->RSSetScissorRects(1, &scissorRect);
			}

			// Present
			{
				CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(backBuffer.Get(),
					D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
				gRHI.g_CommandList->ResourceBarrier(1, &barrier);

				gRHI.g_CommandList->Close();

				gRHI.g_CommandQueue->ExecuteCommandLists(1, CommandListCast(gRHI.g_CommandList.GetAddressOf()));

				UINT syncInterval = gRHI.vsync ? 1 : 0;
				UINT presentFlags = gRHI.supportFeatures.allowTearing && !gRHI.vsync ? DXGI_PRESENT_ALLOW_TEARING : 0;
				gRHI.swapChain->Present(syncInterval, presentFlags);

				gRHI.MoveToNextFrame();
			}

			engine.EndFrame();
		}
	}
	engine.Destroy();
}