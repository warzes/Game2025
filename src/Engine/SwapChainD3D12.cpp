#include "stdafx.h"
#if RENDER_D3D12
#include "SwapChainD3D12.h"
#include "WindowData.h"
#include "Log.h"
#include "Monitor.h"
#include "GPUMarker.h"
//=============================================================================
SwapChainD3D12::~SwapChainD3D12()
{
}
//=============================================================================
bool SwapChainD3D12::Create(const SwapChainD3D12CreateInfo& createInfo)
{
	for (size_t i = 0; i < MAX_BACK_BUFFER_COUNT; i++)
		m_fenceValues[i] = 0;
	m_numBackBuffers = 0;
	m_currentBackBufferIndex = 0;
	m_numTotalFrames = 0;

	assert(createInfo.factory);
	assert(createInfo.device);
	assert(createInfo.allocator);
	assert(createInfo.presentQueue);
	assert(createInfo.RTVStagingDescriptorHeap);
	assert(createInfo.DSVStagingDescriptorHeap);
	assert(createInfo.windowData.hwnd);
	assert(createInfo.numBackBuffers > 0 && createInfo.numBackBuffers <= MAX_BACK_BUFFER_COUNT);

	m_device                   = createInfo.device;
	m_allocator                = createInfo.allocator;
	m_presentQueue             = createInfo.presentQueue;
	m_RTVStagingDescriptorHeap = createInfo.RTVStagingDescriptorHeap;
	m_DSVStagingDescriptorHeap = createInfo.DSVStagingDescriptorHeap;
	m_numBackBuffers           = createInfo.numBackBuffers;
	m_vSync                    = createInfo.vSync;
	m_allowTearing             = createInfo.allowTearing && !m_vSync;

	if (!createSwapChain(createInfo)) return false;

	for (size_t i = 0; i < m_numBackBuffers; i++)
		m_backBuffersDescriptor[i] = m_RTVStagingDescriptorHeap->GetNewDescriptor();

	m_depthStencilDescriptor = m_DSVStagingDescriptorHeap->GetNewDescriptor();

	if (!m_fence.Create(m_device.Get(), "SwapChain Fence")) return false;
	m_fenceValues[m_currentBackBufferIndex]++;

	if (!createRenderTargetViews()) return false;
	if (!createDepthStencilViews(createInfo.windowData.width, createInfo.windowData.height)) return false;

	return true;
}
//=============================================================================
void SwapChainD3D12::Destroy()
{
	WaitForGPU();
	// https://docs.microsoft.com/en-us/windows/win32/direct3d12/swap-chains
	// Full-screen swap chains continue to have the restriction that SetFullscreenState(FALSE, nullptr) must be called before the final release of the swap chain.
	if (m_swapChain) m_swapChain->SetFullscreenState(FALSE, nullptr);

	destroyRenderTargetViews();
	destroyDepthStencilViews();
	for (size_t i = 0; i < MAX_BACK_BUFFER_COUNT; i++)
	{
		if (m_RTVStagingDescriptorHeap) m_RTVStagingDescriptorHeap->FreeDescriptor(m_backBuffersDescriptor[i]);
		m_backBuffers[i].Reset();
	}
	if (m_DSVStagingDescriptorHeap) m_DSVStagingDescriptorHeap->FreeDescriptor(m_depthStencilDescriptor);

	m_fence.Destroy();
	m_swapChain.Reset();
	m_device.Reset();
}
//=============================================================================
bool SwapChainD3D12::Resize(uint32_t width, uint32_t height)
{
	if (!m_swapChain)
	{
		Fatal("SwapChain is null!!!");
		return false;
	}

	WaitForGPU();
	destroyRenderTargetViews();
	destroyDepthStencilViews();
	// Release resources that are tied to the swap chain and update fence values.
	for (int i = 0; i < m_numBackBuffers; i++)
		m_fenceValues[i] = m_fenceValues[m_swapChain->GetCurrentBackBufferIndex()];

	DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
	HRESULT result = m_swapChain->GetDesc(&swapChainDesc);
	if (FAILED(result))
	{
		Fatal("IDXGISwapChain4::GetDesc() failed: " + DXErrorToStr(result));
		return false;
	}

	result = m_swapChain->ResizeBuffers((UINT)m_numBackBuffers, width, height, swapChainDesc.BufferDesc.Format, swapChainDesc.Flags);
	if (FAILED(result))
	{
		Fatal("IDXGISwapChain4::ResizeBuffers() failed: " + DXErrorToStr(result));
		return false;
	}

	if (!createRenderTargetViews()) return false;
	if (!createDepthStencilViews(width, height)) return false;

	m_currentBackBufferIndex = m_swapChain->GetCurrentBackBufferIndex();
	return true;
}
//=============================================================================
bool SwapChainD3D12::Present()
{
	// TODO: glitch detection and avoidance
	// https://docs.microsoft.com/en-us/windows/win32/direct3ddxgi/dxgi-flip-model#avoiding-detecting-and-recovering-from-glitches

	UINT syncInterval = m_vSync ? 1 : 0;
	UINT presentFlags = m_allowTearing ? DXGI_PRESENT_ALLOW_TEARING : 0;

	HRESULT result = m_swapChain->Present(syncInterval, presentFlags);
	if (FAILED(result))
	{
		Fatal("IDXGISwapChain4::Present() failed: " + DXErrorToStr(result));
		return false;
	}

	return true;
}
//=============================================================================
void SwapChainD3D12::MoveToNextFrame()
{
	// Schedule a Signal command in the queue.
	const UINT64 currentFenceValue = m_fenceValues[m_currentBackBufferIndex];
	m_presentQueue->Signal(m_fence, currentFenceValue);

	// Update the frame index.
	m_currentBackBufferIndex = m_swapChain->GetCurrentBackBufferIndex();
	m_numTotalFrames++;
	// If the next frame is not ready to be rendered yet, wait until it is ready.
	m_fence.WaitOnCPU(m_fenceValues[m_currentBackBufferIndex]);
	// Set the fence value for the next frame.
	m_fenceValues[m_currentBackBufferIndex] = currentFenceValue + 1;
}
//=============================================================================
void SwapChainD3D12::WaitForGPU()
{
	if (m_presentQueue->Get() && m_fence.IsValid())
	{
		// Schedule a Signal command in the GPU queue.
		if (m_presentQueue->Signal(m_fence, m_fenceValues[m_currentBackBufferIndex]))
		{
			// Wait until the Signal has been processed.
			if (SUCCEEDED(m_fence.Get()->SetEventOnCompletion(m_fenceValues[m_currentBackBufferIndex], m_fence.GetEvent())))
			{
				std::ignore = WaitForSingleObjectEx(m_fence.GetEvent(), INFINITE, FALSE);

				// Increment the fence value for the current frame.
				m_fenceValues[m_currentBackBufferIndex]++;
			}
		}
	}
}
//=============================================================================
void SwapChainD3D12::SetHDRMetaData(ColorSpace colorSpace, float MaxOutputNits, float MinOutputNits, float MaxContentLightLevel, float MaxFrameAverageLightLevel)
{
	if (!IsHDRFormat())
	{
		Warning("SetHDRMetadata() called on non-HDR swapchain.");
		return;
	}

	// https://en.wikipedia.org/wiki/Chromaticity
	// E.g., the white point of an sRGB display  is an x,y chromaticity of 
	//       (0.3216, 0.3290) where x and y coords are used in the xyY space.
	static const DisplayChromaticities DisplayChromaticityList[] =
	{
		//                    Red_x   , Red_y   , Green_x , Green_y,  Blue_x  , Blue_y  , White_x , White_y    // xyY space
		DisplayChromaticities(0.64000f, 0.33000f, 0.30000f, 0.60000f, 0.15000f, 0.06000f, 0.31270f, 0.32900f), // Display Gamut Rec709 
		DisplayChromaticities(0.70800f, 0.29200f, 0.17000f, 0.79700f, 0.13100f, 0.04600f, 0.31270f, 0.32900f), // Display Gamut Rec2020
		DisplayChromaticities(), // Display Gamut DCI-P3 | TODO: handle p3
	};

	// Set HDR meta data : https://docs.microsoft.com/en-us/windows/win32/api/dxgi1_5/ns-dxgi1_5-dxgi_hdr_metadata_hdr10
	const DisplayChromaticities& Chroma = DisplayChromaticityList[static_cast<size_t>(colorSpace)];
	m_HDRMetaData.RedPrimary[0]             = static_cast<UINT16>(Chroma.RedPrimary_xy[0] * 50000.0f);
	m_HDRMetaData.RedPrimary[1]             = static_cast<UINT16>(Chroma.RedPrimary_xy[1] * 50000.0f);
	m_HDRMetaData.GreenPrimary[0]           = static_cast<UINT16>(Chroma.GreenPrimary_xy[0] * 50000.0f);
	m_HDRMetaData.GreenPrimary[1]           = static_cast<UINT16>(Chroma.GreenPrimary_xy[1] * 50000.0f);
	m_HDRMetaData.BluePrimary[0]            = static_cast<UINT16>(Chroma.BluePrimary_xy[0] * 50000.0f);
	m_HDRMetaData.BluePrimary[1]            = static_cast<UINT16>(Chroma.BluePrimary_xy[1] * 50000.0f);
	m_HDRMetaData.WhitePoint[0]             = static_cast<UINT16>(Chroma.WhitePoint_xy[0] * 50000.0f);
	m_HDRMetaData.WhitePoint[1]             = static_cast<UINT16>(Chroma.WhitePoint_xy[1] * 50000.0f);
	m_HDRMetaData.MaxMasteringLuminance     = static_cast<UINT>(MaxOutputNits * 10000.0f);
	m_HDRMetaData.MinMasteringLuminance     = static_cast<UINT>(MinOutputNits * 10000.0f);
	m_HDRMetaData.MaxContentLightLevel      = static_cast<UINT16>(MaxContentLightLevel);
	m_HDRMetaData.MaxFrameAverageLightLevel = static_cast<UINT16>(MaxFrameAverageLightLevel);
	HRESULT result = m_swapChain->SetHDRMetaData(DXGI_HDR_METADATA_TYPE_HDR10, sizeof(DXGI_HDR_METADATA_HDR10), &m_HDRMetaData);
	if (FAILED(result))
	{
		Fatal("SwapChainD3D12::SetHDRMetaData() failed: " + DXErrorToStr(result));
		return;
	}

	Print("SetHDRMetaData(): Min: " + std::to_string(MinOutputNits) + ", Max: " + std::to_string(MaxOutputNits) + ", MaxCLL: " + std::to_string(MaxContentLightLevel) + ", MaxFALL: " + std::to_string(MaxFrameAverageLightLevel));
}
//=============================================================================
void SwapChainD3D12::ClearHDRMetaData()
{
	m_swapChain->SetHDRMetaData(DXGI_HDR_METADATA_TYPE_NONE, 0, nullptr);
}
//=============================================================================
void SwapChainD3D12::EnsureSwapChainColorSpace(SwapChainBitDepth swapChainBitDepth, bool enableST2084)
{
	DXGI_COLOR_SPACE_TYPE colorSpace = {};
	switch (swapChainBitDepth)
	{
	case SwapChainBitDepth::_8:  colorSpace = DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709; break;
	case SwapChainBitDepth::_10: colorSpace = enableST2084 ? DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020 : DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709; break;
	case SwapChainBitDepth::_16: colorSpace = DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709; break;
	}

	{
		UINT colorSpaceSupport = 0;
		if (SUCCEEDED(m_swapChain->CheckColorSpaceSupport(colorSpace, &colorSpaceSupport)) && ((colorSpaceSupport & DXGI_SWAP_CHAIN_COLOR_SPACE_SUPPORT_FLAG_PRESENT) == DXGI_SWAP_CHAIN_COLOR_SPACE_SUPPORT_FLAG_PRESENT))
		{
			HRESULT result = m_swapChain->SetColorSpace1(colorSpace);
			if (FAILED(result))
			{
				Fatal("SwapChainD3D12::SetColorSpace1() failed: " + DXErrorToStr(result));
				return;
			}
			
			m_colorSpace = colorSpace;
		}
	}
}
//=============================================================================
DXGI_OUTPUT_DESC1 SwapChainD3D12::GetContainingMonitorDesc() const
{
	DXGI_OUTPUT_DESC1 d = {};

	// Figure out which monitor swapChain is in and set the mode we want to use for ResizeTarget().
	IDXGIOutput6* pOut = nullptr;
	{
		IDXGIOutput* pOutput = nullptr;
		HRESULT hr = m_swapChain->GetContainingOutput(&pOutput);
		if (hr != S_OK || pOutput == nullptr)
		{
			Fatal("SwapChain::GetContainingOutput() returned error");
			return d;
		}
		hr = pOutput->QueryInterface(IID_PPV_ARGS(&pOut));
		assert(hr == S_OK);
		pOutput->Release();
	}
	pOut->GetDesc1(&d);
	pOut->Release();
	return d;
}
//=============================================================================
bool SwapChainD3D12::createSwapChain(const SwapChainD3D12CreateInfo& createInfo)
{
	// Determine SwapChain Format based on whether HDR is supported & enabled or not
	DXGI_FORMAT swapChainFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

	// Check HDR support : https://docs.microsoft.com/en-us/windows/win32/direct3darticles/high-dynamic-range
	const bool isHDRCapableDisplayAvailable = checkHDRSupport(createInfo.windowData.hwnd, createInfo.factory);
	if (createInfo.HDR)
	{
		if (isHDRCapableDisplayAvailable)
		{
			switch (createInfo.bitDepth)
			{
			case SwapChainBitDepth::_10:
				Fatal("HDR10 isn't supported for now");
				return false;
				break;
			case SwapChainBitDepth::_16:
				// By default, a swap chain created with a floating point pixel format is treated as if it  uses the DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709 color space, which is also known as scRGB.
				swapChainFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
				m_colorSpace = DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709; // set mColorSpace value to ensure consistant state
				break;
			}
		}
		else
		{
			Warning("No HDR capable display found! Falling back to SDR swapchain.");
			swapChainFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
			m_colorSpace = DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;
		}
	}

	// https://docs.microsoft.com/en-us/windows/win32/direct3ddxgi/dxgi-flip-model
	// https://docs.microsoft.com/en-us/windows/win32/direct3ddxgi/dxgi-1-4-improvements
	// DXGI_SWAP_EFFECT_FLIP_DISCARD    should be preferred when applications fully render over the backbuffer before 
	//                                  presenting it or are interested in supporting multi-adapter scenarios easily.
	// DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL should be used by applications that rely on partial presentation optimizations 
	//                                  or regularly read from previously presented backbuffers.
	constexpr DXGI_SWAP_EFFECT swapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
	swapChainDesc.Width                 = createInfo.windowData.width;
	swapChainDesc.Height                = createInfo.windowData.height;
	swapChainDesc.Format                = swapChainFormat;
	swapChainDesc.Stereo                = FALSE;
	swapChainDesc.SampleDesc            = { 1, 0 };
	swapChainDesc.BufferUsage           = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.BufferCount           = createInfo.numBackBuffers;
	swapChainDesc.Scaling               = DXGI_SCALING_STRETCH;
	swapChainDesc.SwapEffect            = swapEffect;
	swapChainDesc.AlphaMode             = DXGI_ALPHA_MODE_IGNORE;
	swapChainDesc.Flags = m_allowTearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

	DXGI_SWAP_CHAIN_FULLSCREEN_DESC fsSwapChainDesc = {};
	fsSwapChainDesc.Windowed = TRUE;

	ComPtr<IDXGISwapChain1> swapChain1;
	HRESULT result = createInfo.factory->CreateSwapChainForHwnd(*m_presentQueue, createInfo.windowData.hwnd, &swapChainDesc, &fsSwapChainDesc, nullptr, &swapChain1);
	if (FAILED(result))
	{
		Fatal("IDXGIFactory2::CreateSwapChainForHwnd() failed: " + DXErrorToStr(result));
		return false;
	}
	m_backBufferFormat = swapChainFormat;

	result = createInfo.factory->MakeWindowAssociation(createInfo.windowData.hwnd, DXGI_MWA_NO_ALT_ENTER);
	if (FAILED(result))
	{
		Fatal("IDXGIFactory2::MakeWindowAssociation() failed: " + DXErrorToStr(result));
		return false;
	}

	result = swapChain1.As(&m_swapChain);
	if (FAILED(result))
	{
		Fatal("IDXGISwapChain1::QueryInterface() failed: " + DXErrorToStr(result));
		return false;
	}

	// Set color space for HDR if specified
	if (createInfo.HDR && isHDRCapableDisplayAvailable)
	{
		constexpr bool isOutputSignalST2084 = false; // output signal type for the HDR10 standard
		const bool isHDR10Signal = createInfo.bitDepth == SwapChainBitDepth::_10 && isOutputSignalST2084;
		EnsureSwapChainColorSpace(createInfo.bitDepth, isHDR10Signal);
	}

	m_currentBackBufferIndex = m_swapChain->GetCurrentBackBufferIndex();

	return true;
}
//=============================================================================
bool SwapChainD3D12::createRenderTargetViews()
{
	HRESULT result = {};
	for (int bufferIndex = 0; bufferIndex < m_numBackBuffers; bufferIndex++)
	{
		assert(!m_backBuffers[bufferIndex]);

		result = m_swapChain->GetBuffer(bufferIndex, IID_PPV_ARGS(&m_backBuffers[bufferIndex]));
		if (FAILED(result))
		{
			Fatal("IDXGISwapChain4->GetBuffer(" + std::to_string(bufferIndex) + ") failed: " + DXErrorToStr(result));
			return false;
		}

		D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
		rtvDesc.Format = m_backBufferFormat;
		rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

		m_device->CreateRenderTargetView(m_backBuffers[bufferIndex].Get(), &rtvDesc, m_backBuffersDescriptor[bufferIndex].CPUHandle);
		SetName(m_backBuffers[bufferIndex].Get(), "SwapChain::RenderTarget[%d]", bufferIndex);
	}
	return true;
}
//=============================================================================
bool SwapChainD3D12::createDepthStencilViews(uint32_t width, uint32_t height)
{
	if (m_depthBufferFormat != DXGI_FORMAT_UNKNOWN)
	{
		assert(!m_depthStencil);

		D3D12MA::ALLOCATION_DESC depthStencilAllocDesc = {};
		depthStencilAllocDesc.HeapType                 = D3D12_HEAP_TYPE_DEFAULT;

		D3D12_RESOURCE_DESC depthStencilResourceDesc = {};
		depthStencilResourceDesc.Dimension           = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		depthStencilResourceDesc.Alignment           = 0;
		depthStencilResourceDesc.Width               = width;
		depthStencilResourceDesc.Height              = height;
		depthStencilResourceDesc.DepthOrArraySize    = 1;
		depthStencilResourceDesc.MipLevels           = 1;
		depthStencilResourceDesc.Format              = m_depthBufferFormat;
		depthStencilResourceDesc.SampleDesc          = { 1, 0 };
		depthStencilResourceDesc.Layout              = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		depthStencilResourceDesc.Flags               = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

		const CD3DX12_CLEAR_VALUE depthOptimizedClearValue(m_depthBufferFormat, 1.0f, 0u);

		HRESULT result = m_allocator->CreateResource(&depthStencilAllocDesc, &depthStencilResourceDesc, D3D12_RESOURCE_STATE_DEPTH_WRITE, &depthOptimizedClearValue, &m_depthStencilAllocation, IID_PPV_ARGS(&m_depthStencil));
		if (FAILED(result))
		{
			Fatal("D3D12MA::Allocator::CreateResource() failed: " + DXErrorToStr(result));
			return false;
		}
		m_depthStencil->SetName(L"Depth/Stencil Resource Heap");
		m_depthStencilAllocation->SetName(L"Depth/Stencil Resource Heap");

		D3D12_DEPTH_STENCIL_VIEW_DESC depthStencilDesc = {};
		depthStencilDesc.Format                        = m_depthBufferFormat;
		depthStencilDesc.ViewDimension                 = D3D12_DSV_DIMENSION_TEXTURE2D;
		depthStencilDesc.Flags                         = D3D12_DSV_FLAG_NONE;
		m_device->CreateDepthStencilView(m_depthStencil.Get(), &depthStencilDesc, m_depthStencilDescriptor.CPUHandle);
	}

	return true;
}
//=============================================================================
void SwapChainD3D12::destroyRenderTargetViews()
{
	for (int i = 0; i < MAX_BACK_BUFFER_COUNT; i++)
		m_backBuffers[i].Reset();
}
//=============================================================================
void SwapChainD3D12::destroyDepthStencilViews()
{
	m_depthStencil.Reset();
	if (m_depthStencilAllocation) m_depthStencilAllocation->Release(); m_depthStencilAllocation = nullptr;
}
//=============================================================================
// To detect HDR support, we will need to check the color space in the primary DXGI output associated with the app at this point in time (using window/display intersection). 
inline int ComputeIntersectionArea(int ax1, int ay1, int ax2, int ay2, int bx1, int by1, int bx2, int by2)
{
	// Compute the overlay area of two rectangles, A and B.
	// (ax1, ay1) = left-top coordinates of A; (ax2, ay2) = right-bottom coordinates of A
	// (bx1, by1) = left-top coordinates of B; (bx2, by2) = right-bottom coordinates of B
	return std::max(0, std::min(ax2, bx2) - std::max(ax1, bx1)) * std::max(0, std::min(ay2, by2) - std::max(ay1, by1));
}
//=============================================================================
bool SwapChainD3D12::checkHDRSupport(HWND hwnd, ComPtr<IDXGIFactory7> factory)
{
	RECT windowRect = {};
	GetWindowRect(hwnd, &windowRect);

	// From D3D12HDR: 
	// First, the method must determine the app's current display.
	// We don't recommend using IDXGISwapChain::GetContainingOutput method to do that because of two reasons:
	//    1. Swap chains created with CreateSwapChainForComposition do not support this method.
	//    2. Swap chains will return a stale dxgi output once DXGIFactory::IsCurrent() is false. In addition, 
	//       we don't recommend re-creating swapchain to resolve the stale dxgi output because it will cause a short 
	//       period of black screen.
	// Instead, we suggest enumerating through the bounds of all dxgi outputs and determine which one has the greatest 
	// intersection with the app window bounds. Then, use the DXGI output found in previous step to determine if the 
	// app is on a HDR capable display.

	// Retrieve the current default adapter.
	ComPtr<IDXGIAdapter1> dxgiAdapter; // TODO: почему берется первый адаптер, а не основной?
	HRESULT result = factory->EnumAdapters1(0, &dxgiAdapter);
	if (FAILED(result))
	{
		Fatal("IDXGIFactory7::EnumAdapters1() failed: " + DXErrorToStr(result));
		return false;
	}

	// Iterate through the DXGI outputs associated with the DXGI adapter,
	// and find the output whose bounds have the greatest overlap with the
	// app window (i.e. the output for which the intersection area is the
	// greatest).

	UINT i = 0;
	ComPtr<IDXGIOutput> currentOutput;
	ComPtr<IDXGIOutput> bestOutput;
	float bestIntersectArea = -1;

	while (dxgiAdapter->EnumOutputs(i, &currentOutput) != DXGI_ERROR_NOT_FOUND)
	{
		// Get the rectangle bounds of the app window
		int ax1 = windowRect.left;
		int ay1 = windowRect.top;
		int ax2 = windowRect.right;
		int ay2 = windowRect.bottom;

		// Get the rectangle bounds of current output
		DXGI_OUTPUT_DESC desc;
		result = currentOutput->GetDesc(&desc);
		if (FAILED(result))
		{
			Fatal("IDXGIOutput::GetDesc() failed: " + DXErrorToStr(result));
			return false;
		}
		RECT r = desc.DesktopCoordinates;
		int bx1 = r.left;
		int by1 = r.top;
		int bx2 = r.right;
		int by2 = r.bottom;

		// Compute the intersection
		int intersectArea = ComputeIntersectionArea(ax1, ay1, ax2, ay2, bx1, by1, bx2, by2);
		if (intersectArea > bestIntersectArea)
		{
			bestOutput = currentOutput;
			bestIntersectArea = static_cast<float>(intersectArea);
		}

		i++;
	}

	// Having determined the output (display) upon which the app is primarily being rendered, retrieve the HDR capabilities of that display by checking the color space.
	ComPtr<IDXGIOutput6> output6;
	result = bestOutput.As(&output6);
	if (FAILED(result))
	{
		Fatal("IDXGIOutput as IDXGIOutput6 failed: " + DXErrorToStr(result));
		return false;
	}

	DXGI_OUTPUT_DESC1 desc1;
	result = output6->GetDesc1(&desc1);
	if (FAILED(result))
	{
		Fatal("IDXGIOutput6::GetDesc1() failed: " + DXErrorToStr(result));
		return false;
	}

	return (desc1.ColorSpace == DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020);
}
//=============================================================================
#endif // RENDER_D3D12