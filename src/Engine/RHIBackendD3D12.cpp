﻿#include "stdafx.h"
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
	resetVal();
	setSize(wndData.width, wndData.height);

	if (!context.Create(createInfo.context)) return false;
	if (!commandQueue.Create(context.GetD3DDeviceRef(), D3D12_COMMAND_LIST_TYPE_DIRECT, "Main Render Command Queue")) return false;
	if (!createDescriptorHeap()) return false;
	depthStencilDescriptor = DSVStagingDescriptorHeap->GetNewDescriptor();

	SwapChainD3D12CreateInfo swapChainCreateInfo = { .windowData = wndData };
	swapChainCreateInfo.factory = context.GetD3DFactory();
	swapChainCreateInfo.device = context.GetD3DDevice();
	swapChainCreateInfo.presentQueue = &commandQueue;
	swapChainCreateInfo.RTVStagingDescriptorHeap = RTVStagingDescriptorHeap;
	swapChainCreateInfo.allowTearing = context.IsSupportAllowTearing();
	swapChainCreateInfo.vSync = createInfo.vsync;
	if (!swapChain.Create(swapChainCreateInfo)) return false;

	if (!updateRenderTargetViews()) return false;

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
	commandQueue.Destroy();

	destroyRenderTargetViews();
	if (DSVStagingDescriptorHeap) DSVStagingDescriptorHeap->FreeDescriptor(depthStencilDescriptor);

	swapChain.Destroy();

	delete RTVStagingDescriptorHeap; RTVStagingDescriptorHeap = nullptr;
	delete DSVStagingDescriptorHeap; DSVStagingDescriptorHeap = nullptr;
	delete CBVSRVUAVStagingDescriptorHeap; CBVSRVUAVStagingDescriptorHeap = nullptr;

	frameBufferWidth = 0;
	frameBufferHeight = 0;

	context.Destroy();
}
//=============================================================================
void RHIBackend::ResizeFrameBuffer(uint32_t width, uint32_t height)
{
	if (!setSize(width, height)) return;

	// Wait until all previous GPU work is complete.
	WaitForGpu();

	destroyRenderTargetViews();
	if (!swapChain.Resize(width, height)) return;
	updateRenderTargetViews();

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
//=============================================================================
void RHIBackend::Present(D3D12_RESOURCE_STATES beforeState)
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
	commandQueue.Get()->ExecuteCommandLists(1, CommandListCast(commandList.GetAddressOf()));

	swapChain.Present();

	moveToNextFrame();
}
//=============================================================================
void RHIBackend::WaitForGpu()
{
	swapChain.WaitForGPU();
}
//=============================================================================
void RHIBackend::resetVal()
{
	frameBufferWidth = 0;
	frameBufferHeight = 0;
}
//=============================================================================
bool RHIBackend::setSize(uint32_t width, uint32_t height)
{
	// Don't allow 0 size swap chain back buffers.
	width = std::max(1u, width);
	height = std::max(1u, height);
	if (frameBufferWidth == width && frameBufferHeight == height) return false;
	frameBufferWidth = width;
	frameBufferHeight = height;

	return true;
}
//=============================================================================
bool RHIBackend::createDescriptorHeap()
{
	RTVStagingDescriptorHeap = new StagingDescriptorHeapD3D12(GetD3DDevice(), D3D12_DESCRIPTOR_HEAP_TYPE_RTV, NUM_RTV_STAGING_DESCRIPTORS);
	if (IsRequestExit())
	{
		Fatal("Create RTVStagingDescriptorHeap failed.");
		return false;
	}
	DSVStagingDescriptorHeap = new StagingDescriptorHeapD3D12(GetD3DDevice(), D3D12_DESCRIPTOR_HEAP_TYPE_DSV, NUM_DSV_STAGING_DESCRIPTORS);
	if (IsRequestExit())
	{
		Fatal("Create DSVStagingDescriptorHeap failed.");
		return false;
	}
	CBVSRVUAVStagingDescriptorHeap = new StagingDescriptorHeapD3D12(GetD3DDevice(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, NUM_SRV_STAGING_DESCRIPTORS);
	if (IsRequestExit())
	{
		Fatal("Create CBVSRVUAVStagingDescriptorHeap failed.");
		return false;
	}
	return true;
}
//=============================================================================
bool RHIBackend::updateRenderTargetViews()
{
	if (depthBufferFormat != DXGI_FORMAT_UNKNOWN)
	{
		assert(!depthStencil);

		// Allocate a 2-D surface as the depth/stencil buffer and create a depth/stencil view on this surface.
		const CD3DX12_HEAP_PROPERTIES depthHeapProperties(D3D12_HEAP_TYPE_DEFAULT);

		const D3D12_RESOURCE_DESC depthStencilDesc = CD3DX12_RESOURCE_DESC::Tex2D(
			depthBufferFormat, frameBufferWidth, frameBufferHeight, 1, 0, 1, 0,
			D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL
		);

		const CD3DX12_CLEAR_VALUE depthOptimizedClearValue(depthBufferFormat, 1.0f, 0u);

		HRESULT result = GetD3DDevice()->CreateCommittedResource(
			&depthHeapProperties,
			D3D12_HEAP_FLAG_NONE,
			&depthStencilDesc,
			D3D12_RESOURCE_STATE_DEPTH_WRITE,
			&depthOptimizedClearValue,
			IID_PPV_ARGS(depthStencil.ReleaseAndGetAddressOf())
		);
		if (FAILED(result))
		{
			Fatal("ID3D12Device14::CreateCommittedResource() failed: " + DXErrorToStr(result));
			return false;
		}

		depthStencil->SetName(L"Depth stencil");

		D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
		dsvDesc.Format                        = depthBufferFormat;
		dsvDesc.ViewDimension                 = D3D12_DSV_DIMENSION_TEXTURE2D;

		GetD3DDevice()->CreateDepthStencilView(depthStencil.Get(), &dsvDesc, depthStencilDescriptor.CPUHandle);
	}

	// Set the 3D rendering viewport and scissor rectangle to target the entire window.
	screenViewport.TopLeftX = screenViewport.TopLeftY = 0.f;
	screenViewport.Width = static_cast<float>(frameBufferWidth);
	screenViewport.Height = static_cast<float>(frameBufferHeight);
	screenViewport.MinDepth = D3D12_MIN_DEPTH;
	screenViewport.MaxDepth = D3D12_MAX_DEPTH;

	scissorRect.left = scissorRect.top = 0;
	scissorRect.right = static_cast<LONG>(frameBufferWidth);
	scissorRect.bottom = static_cast<LONG>(frameBufferHeight);

	return true;
}
//=============================================================================
void RHIBackend::destroyRenderTargetViews()
{
	depthStencil.Reset();
}
//=============================================================================
void RHIBackend::moveToNextFrame()
{
	swapChain.MoveToNextFrame();
}
//=============================================================================
#endif // RENDER_D3D12