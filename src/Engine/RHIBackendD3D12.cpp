#include "stdafx.h"
#if RENDER_D3D12
#include "RHIBackendD3D12.h"
#include "WindowData.h"
#include "Log.h"
#include "RenderCore.h"
//=============================================================================
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")
//=============================================================================
extern "C" { __declspec(dllexport) extern const UINT D3D12SDKVersion = D3D12_PREVIEW_SDK_VERSION; }
extern "C" { __declspec(dllexport) extern const char* D3D12SDKPath = ".\\D3D12\\"; }
//=============================================================================
RHIBackend gRHI{};
//=============================================================================
bool RHIBackend::CreateAPI(const WindowData& wndData, const RenderSystemCreateInfo& createInfo)
{
	if (!context.Create(createInfo.context)) return false;
	if (!createCommandQueue()) return false;
	if (!descriptorHeapManager.Create(context.GetD3DDevice())) return false;

	SwapChainD3D12CreateInfo swapChainCreateInfo = { 
		.windowData = wndData, 
		.context = context,
		.descriptorHeapManager = descriptorHeapManager };
	swapChainCreateInfo.presentQueue = &graphicsQueue;
	swapChainCreateInfo.vSync        = createInfo.vsync;
	if (!swapChain.Create(swapChainCreateInfo)) return false;

	// Create a command allocator for each back buffer that will be rendered to.
	for (int i = 0; i < MAX_BACK_BUFFER_COUNT; ++i)
	{
		HRESULT result = GetD3DDevice()->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocators[i]));
		if (FAILED(result))
		{
			Fatal("ID3D12Device14::CreateCommandAllocator() failed: " + DXErrorToStr(result));
			return false;
		}
		wchar_t name[25] = {};
		swprintf_s(name, L"Render target %u", i);
		commandAllocators[i]->SetName(name);
	}

	// Create a command list for recording graphics commands.
	HRESULT result = GetD3DDevice()->CreateCommandList1(0, D3D12_COMMAND_LIST_TYPE_DIRECT, D3D12_COMMAND_LIST_FLAG_NONE, IID_PPV_ARGS(&commandList));
	if (FAILED(result))
	{
		Fatal("ID3D12Device14::CreateCommandList1() failed: " + DXErrorToStr(result));
		return false;
	}

	return true;
}
//=============================================================================
void RHIBackend::DestroyAPI()
{
	WaitForGpu();

	for (size_t i = 0; i < MAX_BACK_BUFFER_COUNT; i++)
	{
		commandAllocators[i].Reset();
	}
	commandList.Reset();
	copyQueue.Destroy();
	computeQueue.Destroy();
	graphicsQueue.Destroy();

	swapChain.Destroy();

	descriptorHeapManager.Destroy();

	context.Destroy();
}
//=============================================================================
void RHIBackend::ResizeFrameBuffer(uint32_t width, uint32_t height)
{
	// Wait until all previous GPU work is complete.
	WaitForGpu();
	if (!swapChain.Resize(width, height)) return;
	Print("Window Resize: " + std::to_string(width) + "." + std::to_string(height));
}
//=============================================================================
void RHIBackend::BeginFrame()
{
}
//=============================================================================
void RHIBackend::EndFrame()
{
}
//=============================================================================
void RHIBackend::Prepare(D3D12_RESOURCE_STATES beforeState, D3D12_RESOURCE_STATES afterState)
{
	PIXBeginEvent(PIX_COLOR_DEFAULT, L"Prepare");
	{
		// Reset command list and allocator.
		HRESULT result = commandAllocators[swapChain.GetCurrentBackBufferIndex()]->Reset();
		if (FAILED(result))
		{
			Fatal("ID3D12CommandAllocator::Reset() failed: " + DXErrorToStr(result));
			return;
		}

		result = commandList->Reset(commandAllocators[swapChain.GetCurrentBackBufferIndex()].Get(), nullptr);
		if (FAILED(result))
		{
			Fatal("ID3D12GraphicsCommandList10::Reset() failed: " + DXErrorToStr(result));
			return;
		}

		if (beforeState != afterState)
		{
			const D3D12_RESOURCE_BARRIER barrierRTV = CD3DX12_RESOURCE_BARRIER::Transition(swapChain.GetCurrentBackBufferRenderTarget(),
				beforeState, afterState);
			commandList->ResourceBarrier(1, &barrierRTV);
		}
	}
	PIXEndEvent();
}
//=============================================================================
void RHIBackend::Present(D3D12_RESOURCE_STATES beforeState)
{
	PIXBeginEvent(PIX_COLOR_DEFAULT, L"Present");
	{
		if (beforeState != D3D12_RESOURCE_STATE_PRESENT)
		{
			// Transition the render target to the state that allows it to be presented to the display.
			const D3D12_RESOURCE_BARRIER barrierRTV = CD3DX12_RESOURCE_BARRIER::Transition(
				swapChain.GetCurrentBackBufferRenderTarget(),
				beforeState, D3D12_RESOURCE_STATE_PRESENT);
			commandList->ResourceBarrier(1, &barrierRTV);
		}

		// Send the command list off to the GPU for processing.
		HRESULT result = commandList->Close();
		if (FAILED(result))
		{
			Fatal("ID3D12GraphicsCommandList10::Close() failed: " + DXErrorToStr(result));
			return;
		}
		graphicsQueue.ExecuteCommandList(commandList);

		swapChain.Present();
		swapChain.MoveToNextFrame();
	}
	PIXEndEvent();
}
//=============================================================================
void RHIBackend::WaitForGpu()
{
	swapChain.WaitForGPU();
}
//=============================================================================
void RHIBackend::ClearFrameBuffer(const glm::vec4& color)
{
	PIXBeginEvent(commandList.Get(), PIX_COLOR_DEFAULT, L"ClearFrameBuffer");

	const auto rtvDescriptor = swapChain.GetRenderTargetView();
	const auto dsvDescriptor = swapChain.GetDepthStencilView();
	const auto viewport = swapChain.GetScreenViewport();
	const auto scissorRect = swapChain.GetScissorRect();

	commandList->OMSetRenderTargets(1, &rtvDescriptor, FALSE, &dsvDescriptor);
	commandList->ClearRenderTargetView(rtvDescriptor, &color[0], 0, nullptr);
	commandList->ClearDepthStencilView(dsvDescriptor, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
	commandList->RSSetViewports(1, &viewport);
	commandList->RSSetScissorRects(1, &scissorRect);

	PIXEndEvent(commandList.Get());
}
//=============================================================================
bool RHIBackend::createCommandQueue()
{
	if (!graphicsQueue.Create(context.GetD3DDeviceRef(), D3D12_COMMAND_LIST_TYPE_DIRECT, "Main Graphics Command Queue")) return false;

	if (!computeQueue.Create(context.GetD3DDeviceRef(), D3D12_COMMAND_LIST_TYPE_COMPUTE, "Main Compute Command Queue")) return false;

	if (!copyQueue.Create(context.GetD3DDeviceRef(), D3D12_COMMAND_LIST_TYPE_COPY, "Main Copy Command Queue")) return false;

	return true;
}
//=============================================================================
#endif // RENDER_D3D12