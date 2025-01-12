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
	setSize(wndData.width, wndData.height);
	vsync = createInfo.vsync;

	if (!context.Create(createInfo.context)) return false;
	supportFeatures.allowTearing = context.IsSupportAllowTearing();

	if (!commandQueue.Create(context.GetD3DDevice(), D3D12_COMMAND_LIST_TYPE_DIRECT, "Main Render Command Queue")) return false;

	if (!createDescriptorHeap()) return false;
	if (!createSwapChain(wndData)) return false;

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

	// Create a fence for tracking GPU execution progress.
	if (!fence.Create(context.GetD3DDevice(), "Main Render Fence")) return false;	
	for (size_t i = 0; i < MAX_BACK_BUFFER_COUNT; i++)
		fenceValues[i] = 0;
	//fenceValues[currentBackBufferIndex]++; // TODO: ненужно?
	
	if (!updateRenderTargetViews()) return false;

	return true;
}
//=============================================================================
void RHIBackend::DestroyAPI()
{
	WaitForGpu();

	for (size_t i = 0; i < MAX_BACK_BUFFER_COUNT; i++)
	{
		fenceValues[i] = 0;
		commandAllocators[i].Reset();
	}
	commandList.Reset();
	commandQueue.Destroy();
	fence.Destroy();

	destroyRenderTargetViews();

	delete RTVStagingDescriptorHeap; RTVStagingDescriptorHeap = nullptr;
	delete DSVStagingDescriptorHeap; DSVStagingDescriptorHeap = nullptr;
	delete CBVSRVUAVStagingDescriptorHeap; CBVSRVUAVStagingDescriptorHeap = nullptr;

	currentBackBufferIndex = 0;
	frameBufferWidth = 0;
	frameBufferHeight = 0;

	swapChain.Reset();

	context.Destroy();
}
//=============================================================================
void RHIBackend::ResizeFrameBuffer(uint32_t width, uint32_t height)
{
	if (!setSize(width, height)) return;

	// Wait until all previous GPU work is complete.
	WaitForGpu();

	destroyRenderTargetViews();

	// Release resources that are tied to the swap chain and update fence values.
	for (UINT n = 0; n < MAX_BACK_BUFFER_COUNT; n++)
	{
		//fenceValues[n] = fenceValues[currentBackBufferIndex];
		fenceValues[n] = 0;
	}

	DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
	HRESULT result = swapChain->GetDesc(&swapChainDesc);
	if (FAILED(result))
	{
		Fatal("IDXGISwapChain4::GetDesc() failed: " + DXErrorToStr(result));
		return;
	}
	
	result = swapChain->ResizeBuffers(MAX_BACK_BUFFER_COUNT, frameBufferWidth, frameBufferHeight, swapChainDesc.BufferDesc.Format, swapChainDesc.Flags);
	if (FAILED(result))
	{
		Fatal("IDXGISwapChain4::ResizeBuffers() failed: " + DXErrorToStr(result));
		return;
	}
	updateRenderTargetViews();

	currentBackBufferIndex = swapChain->GetCurrentBackBufferIndex();

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
	HRESULT result = commandAllocators[currentBackBufferIndex]->Reset();
	if (FAILED(result))
	{
		Fatal("ID3D12CommandAllocator::Reset() failed: " + DXErrorToStr(result));
		return;
	}

	result = commandList->Reset(commandAllocators[currentBackBufferIndex].Get(), nullptr);
	if (FAILED(result))
	{
		Fatal("ID3D12GraphicsCommandList10::Reset() failed: " + DXErrorToStr(result));
		return;
	}

	if (beforeState != afterState)
	{
		const D3D12_RESOURCE_BARRIER barrierRTV = CD3DX12_RESOURCE_BARRIER::Transition(
			backBuffers[currentBackBufferIndex].Get(),
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
			backBuffers[currentBackBufferIndex].Get(),
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

	UINT syncInterval = vsync ? 1 : 0;
	UINT presentFlags = supportFeatures.allowTearing && !vsync ? DXGI_PRESENT_ALLOW_TEARING : 0;
	result = swapChain->Present(syncInterval, presentFlags);
	if (FAILED(result))
	{
		Fatal("IDXGISwapChain4::Present() failed: " + DXErrorToStr(result));
	}

	moveToNextFrame();
}
//=============================================================================
void RHIBackend::WaitForGpu()
{
	if (commandQueue.Get() && fence.IsValid())
	{
		// Schedule a Signal command in the GPU queue.
		const UINT64 fenceValue = fenceValues[currentBackBufferIndex];
		if (commandQueue.Signal(fence, fenceValue))
		{
			// Wait until the Signal has been processed.
			if (SUCCEEDED(fence.Get()->SetEventOnCompletion(fenceValue, fence.GetEvent())))
			{
				std::ignore = WaitForSingleObjectEx(fence.GetEvent(), INFINITE, FALSE);

				// Increment the fence value for the current frame.
				fenceValues[currentBackBufferIndex]++;
			}
		}
	}
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
bool RHIBackend::createSwapChain(const WindowData& wndData)
{
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
	swapChainDesc.Width                 = wndData.width;
	swapChainDesc.Height                = wndData.height;
	swapChainDesc.Format                = backBufferFormat;
	swapChainDesc.Stereo                = FALSE;
	swapChainDesc.SampleDesc            = { 1, 0 };
	swapChainDesc.BufferUsage           = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.BufferCount           = MAX_BACK_BUFFER_COUNT;
	swapChainDesc.Scaling               = DXGI_SCALING_STRETCH;
	swapChainDesc.SwapEffect            = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.AlphaMode             = DXGI_ALPHA_MODE_IGNORE;
	swapChainDesc.Flags = supportFeatures.allowTearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

	DXGI_SWAP_CHAIN_FULLSCREEN_DESC fsSwapChainDesc = {};
	fsSwapChainDesc.Windowed = TRUE;

	ComPtr<IDXGISwapChain1> swapChain1;
	HRESULT result = context.GetD3DFactory()->CreateSwapChainForHwnd(commandQueue, wndData.hwnd, &swapChainDesc, &fsSwapChainDesc, nullptr, &swapChain1);
	if (FAILED(result))
	{
		Fatal("IDXGIFactory2::CreateSwapChainForHwnd() failed: " + DXErrorToStr(result));
		return false;
	}

	result = context.GetD3DFactory()->MakeWindowAssociation(wndData.hwnd, DXGI_MWA_NO_ALT_ENTER);
	if (FAILED(result))
	{
		Fatal("IDXGIFactory2::MakeWindowAssociation() failed: " + DXErrorToStr(result));
		return false;
	}

	result = swapChain1.As(&swapChain);
	if (FAILED(result))
	{
		Fatal("IDXGISwapChain1::QueryInterface() failed: " + DXErrorToStr(result));
		return false;
	}

	currentBackBufferIndex = swapChain->GetCurrentBackBufferIndex();

	return true;
}
//=============================================================================
bool RHIBackend::updateRenderTargetViews()
{
	for (uint32_t bufferIndex = 0; bufferIndex < MAX_BACK_BUFFER_COUNT; bufferIndex++)
	{
		assert(!backBuffersDescriptor[bufferIndex].IsValid());
		assert(!backBuffers[bufferIndex]);

		HRESULT result = swapChain->GetBuffer(bufferIndex, IID_PPV_ARGS(&backBuffers[bufferIndex]));
		if (FAILED(result))
		{
			Fatal("IDXGISwapChain4::GetBuffer() failed: " + DXErrorToStr(result));
			return false;
		}

		wchar_t name[25] = {};
		swprintf_s(name, L"Render target %u", bufferIndex);
		backBuffers[bufferIndex]->SetName(name);

		D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
		rtvDesc.Format                        = backBufferFormat; // TODO: DXGI_FORMAT_R8G8B8A8_UNORM_SRGB ???
		rtvDesc.ViewDimension                 = D3D12_RTV_DIMENSION_TEXTURE2D;


		backBuffersDescriptor[bufferIndex] = RTVStagingDescriptorHeap->GetNewDescriptor();
		GetD3DDevice()->CreateRenderTargetView(backBuffers[bufferIndex].Get(), &rtvDesc, backBuffersDescriptor[bufferIndex].CPUHandle);
	}

	if (depthBufferFormat != DXGI_FORMAT_UNKNOWN)
	{
		assert(!depthStencilDescriptor.IsValid());
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

		depthStencilDescriptor = DSVStagingDescriptorHeap->GetNewDescriptor();

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
	for (size_t i = 0; i < MAX_BACK_BUFFER_COUNT; i++)
	{
		if (RTVStagingDescriptorHeap) RTVStagingDescriptorHeap->FreeDescriptor(backBuffersDescriptor[i]);
		backBuffers[i].Reset();
	}
	if (DSVStagingDescriptorHeap) DSVStagingDescriptorHeap->FreeDescriptor(depthStencilDescriptor);
	depthStencil.Reset();
}
//=============================================================================
void RHIBackend::moveToNextFrame()
{
	// Schedule a Signal command in the queue.
	const UINT64 currentFenceValue = fenceValues[currentBackBufferIndex];
	commandQueue.Signal(fence, currentFenceValue);


	// Update the back buffer index.
	currentBackBufferIndex = swapChain->GetCurrentBackBufferIndex();
	// If the next frame is not ready to be rendered yet, wait until it is ready.
	fence.WaitOnCPU(fenceValues[currentBackBufferIndex]);
	// Set the fence value for the next frame.
	fenceValues[currentBackBufferIndex] = currentFenceValue + 1;
}
//=============================================================================
#endif // RENDER_D3D12