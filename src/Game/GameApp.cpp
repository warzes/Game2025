#include "stdafx.h"
#include "Engine/TempD3D12.h"

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

			auto commandAllocator = gRHI.g_CommandAllocators[gRHI.g_CurrentBackBufferIndex];
			auto backBuffer = gRHI.g_BackBuffers[gRHI.g_CurrentBackBufferIndex];

			commandAllocator->Reset();
			gRHI.g_CommandList->Reset(commandAllocator.Get(), nullptr);

			// Clear the render target.
			{
				CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(backBuffer.Get(),
					D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);

				gRHI.g_CommandList->ResourceBarrier(1, &barrier);
				
				FLOAT clearColor[] = { 0.4f, 0.6f, 0.9f, 1.0f };
				CD3DX12_CPU_DESCRIPTOR_HANDLE rtv(gRHI.g_RTVDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), gRHI.g_CurrentBackBufferIndex, gRHI.g_RTVDescriptorSize);

				gRHI.g_CommandList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);
			}

			// Present
			{
				CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(backBuffer.Get(),
					D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
				gRHI.g_CommandList->ResourceBarrier(1, &barrier);

				gRHI.g_CommandList->Close();

				ID3D12CommandList* const commandLists[] = { gRHI.g_CommandList.Get() };
				gRHI.g_CommandQueue->ExecuteCommandLists(_countof(commandLists), commandLists);

				UINT syncInterval = gRHI.vsync ? 1 : 0;
				UINT presentFlags = gRHI.supportFeatures.allowTearing && !gRHI.vsync ? DXGI_PRESENT_ALLOW_TEARING : 0;
				gRHI.swapChain->Present(syncInterval, presentFlags);

				gRHI.g_FrameFenceValues[gRHI.g_CurrentBackBufferIndex] = Signal(gRHI.g_CommandQueue, gRHI.g_Fence, gRHI.g_FenceValue);
				gRHI.g_CurrentBackBufferIndex = gRHI.swapChain->GetCurrentBackBufferIndex();

				WaitForFenceValue(gRHI.g_Fence, gRHI.g_FrameFenceValues[gRHI.g_CurrentBackBufferIndex], gRHI.g_FenceEvent);
			}
			
			engine.EndFrame();
		}
	}
	engine.Destroy();
}