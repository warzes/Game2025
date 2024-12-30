﻿#pragma once

inline ComPtr<ID3D12CommandQueue> CreateCommandQueue(ComPtr<ID3D12Device14> device, D3D12_COMMAND_LIST_TYPE type)
{
	D3D12_COMMAND_QUEUE_DESC desc = {};
	desc.Type = type;
	desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	desc.NodeMask = 0;

	ComPtr<ID3D12CommandQueue> d3d12CommandQueue;
	HRESULT result = device->CreateCommandQueue(&desc, IID_PPV_ARGS(&d3d12CommandQueue));

	return d3d12CommandQueue;
}

inline ComPtr<IDXGISwapChain4> CreateSwapChain(HWND hWnd, ComPtr<ID3D12CommandQueue> commandQueue, uint32_t width, uint32_t height, DXGI_FORMAT backBufferFormat, uint32_t bufferCount, bool tearingSupport)
{
	ComPtr<IDXGISwapChain4> dxgiSwapChain4;
	ComPtr<IDXGIFactory4> dxgiFactory4;
	UINT createFactoryFlags = 0;
#if defined(_DEBUG)
	createFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;
#endif

	HRESULT result = CreateDXGIFactory2(createFactoryFlags, IID_PPV_ARGS(&dxgiFactory4));

	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
	swapChainDesc.Width = width;
	swapChainDesc.Height = height;
	swapChainDesc.Format = backBufferFormat;
	swapChainDesc.Stereo = FALSE;
	swapChainDesc.SampleDesc = { 1, 0 };
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.BufferCount = bufferCount;
	swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
	// It is recommended to always allow tearing if tearing support is available.
	swapChainDesc.Flags = tearingSupport ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

	DXGI_SWAP_CHAIN_FULLSCREEN_DESC fsSwapChainDesc = {};
	fsSwapChainDesc.Windowed = TRUE;

	ComPtr<IDXGISwapChain1> swapChain1;
	result = dxgiFactory4->CreateSwapChainForHwnd(
		commandQueue.Get(),
		hWnd,
		&swapChainDesc,
		&fsSwapChainDesc,
		nullptr,
		&swapChain1);

	swapChain1.As(&dxgiSwapChain4);

	// Disable the Alt+Enter fullscreen toggle feature. Switching to fullscreen will be handled manually.
	result = dxgiFactory4->MakeWindowAssociation(hWnd, DXGI_MWA_NO_ALT_ENTER);

	return dxgiSwapChain4;
}

inline ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(ComPtr<ID3D12Device2> device, D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t numDescriptors)
{
	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	desc.Type = type;
	desc.NumDescriptors = numDescriptors;
	desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	desc.NodeMask = 0;

	ComPtr<ID3D12DescriptorHeap> descriptorHeap;
	HRESULT result = device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&descriptorHeap));

	return descriptorHeap;
}

inline void UpdateRenderTargetViews(ComPtr<ID3D12Device2> device, UINT g_RTVDescriptorSize, ComPtr<IDXGISwapChain4> swapChain, ComPtr<ID3D12DescriptorHeap> descriptorHeap) // TODO: rename
{
	for (int i = 0; i < NUM_BACK_BUFFERS; ++i)
	{
		HRESULT result = swapChain->GetBuffer(i, IID_PPV_ARGS(gRHI.backBuffers[i].GetAddressOf()));
		wchar_t name[40] = {};
		swprintf_s(name, L"MainFrame Render target %u", i);
		gRHI.backBuffers[i]->SetName(name);

		const auto cpuHandle = descriptorHeap->GetCPUDescriptorHandleForHeapStart();
		const CD3DX12_CPU_DESCRIPTOR_HANDLE rtvDescriptor(cpuHandle, static_cast<INT>(i), g_RTVDescriptorSize);

		device->CreateRenderTargetView(gRHI.backBuffers[i].Get(), nullptr, rtvDescriptor);
	}

	// TODO:
	// Allocate a 2-D surface as the depth/stencil buffer and create a depth/stencil view on this surface.
	{
		const CD3DX12_HEAP_PROPERTIES depthHeapProperties(D3D12_HEAP_TYPE_DEFAULT);
		D3D12_RESOURCE_DESC depthStencilDesc = CD3DX12_RESOURCE_DESC::Tex2D(
			gRHI.depthBufferFormat, gRHI.frameBufferWidth, gRHI.frameBufferHeight, 1, 0, 1, 0,
			D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL
		);

		const CD3DX12_CLEAR_VALUE depthOptimizedClearValue(gRHI.depthBufferFormat, 1.0f, 0u);

		device->CreateCommittedResource(
			&depthHeapProperties,
			D3D12_HEAP_FLAG_NONE,
			&depthStencilDesc,
			D3D12_RESOURCE_STATE_DEPTH_WRITE,
			&depthOptimizedClearValue,
			IID_PPV_ARGS(gRHI.depthStencil.ReleaseAndGetAddressOf())
		);
		gRHI.depthStencil->SetName(L"Depth stencil");

		D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
		dsvDesc.Format = gRHI.depthBufferFormat;
		dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		dsvDesc.Texture2D.MipSlice = 0;
		dsvDesc.Flags = D3D12_DSV_FLAG_NONE;

		const auto cpuHandle = gRHI.dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
		device->CreateDepthStencilView(gRHI.depthStencil.Get(), &dsvDesc, cpuHandle);
	}
}

inline ComPtr<ID3D12CommandAllocator> CreateCommandAllocator(ComPtr<ID3D12Device2> device, D3D12_COMMAND_LIST_TYPE type)
{
	ComPtr<ID3D12CommandAllocator> commandAllocator;
	HRESULT result = device->CreateCommandAllocator(type, IID_PPV_ARGS(&commandAllocator));

	return commandAllocator;
}

inline ComPtr<ID3D12GraphicsCommandList> CreateCommandList(ComPtr<ID3D12Device2> device, ComPtr<ID3D12CommandAllocator> commandAllocator, D3D12_COMMAND_LIST_TYPE type)
{
	ComPtr<ID3D12GraphicsCommandList> commandList;
	HRESULT result = device->CreateCommandList(0, type, commandAllocator.Get(), nullptr, IID_PPV_ARGS(&commandList));
	commandList->Close();
	return commandList;
}

inline ComPtr<ID3D12Fence> CreateFence(ComPtr<ID3D12Device2> device)
{
	ComPtr<ID3D12Fence> fence;

	HRESULT result = device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));

	return fence;
}

inline HANDLE CreateEventHandle()
{
	HANDLE fenceEvent;

	fenceEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);
	assert(fenceEvent && "Failed to create fence event.");

	return fenceEvent;
}

inline uint64_t Signal(ComPtr<ID3D12CommandQueue> commandQueue, ComPtr<ID3D12Fence> fence,
	uint64_t& fenceValue)
{
	uint64_t fenceValueForSignal = ++fenceValue;
	commandQueue->Signal(fence.Get(), fenceValueForSignal);

	return fenceValueForSignal;
}

inline void WaitForFenceValue(ComPtr<ID3D12Fence> fence, uint64_t fenceValue, HANDLE fenceEvent, std::chrono::milliseconds duration = std::chrono::milliseconds::max())
{
	if (fence->GetCompletedValue() < fenceValue)
	{
		fence->SetEventOnCompletion(fenceValue, fenceEvent);
		::WaitForSingleObject(fenceEvent, static_cast<DWORD>(duration.count()));
	}
}


// используется чтобы убедиться что все комадны на gpu уже завершились
// например для ресайза
inline void Flush(ComPtr<ID3D12CommandQueue> commandQueue, ComPtr<ID3D12Fence> fence, uint64_t& fenceValue, HANDLE fenceEvent)
{
	uint64_t fenceValueForSignal = Signal(commandQueue, fence, fenceValue);
	WaitForFenceValue(fence, fenceValueForSignal, fenceEvent);
}