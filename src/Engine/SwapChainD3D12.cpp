#include "stdafx.h"
#if RENDER_D3D12
#include "SwapChainD3D12.h"
#include "WindowData.h"
#include "Log.h"
#include "Monitor.h"
//=============================================================================
SwapChainD3D12::~SwapChainD3D12()
{
}
//=============================================================================
bool SwapChainD3D12::Create(const SwapChainD3D12CreateInfo& createInfo)
{
	assert(createInfo.cmdQueue);
	assert(createInfo.factory);
	assert(createInfo.device);
	assert(createInfo.windowData->hwnd);
	assert(createInfo.numBackBuffers > 0 && createInfo.numBackBuffers <= MAX_BACK_BUFFER_COUNT);

	m_device = createInfo.device;
	m_presentQueue = createInfo.cmdQueue;
	m_numBackBuffers = createInfo.numBackBuffers;
	m_vSync = createInfo.vSync;
	m_allowTearing = createInfo.allowTearing;

	// Determine Swapchain Format based on whether HDR is supported & enabled or not
	DXGI_FORMAT SwapChainFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

	// Check HDR support : https://docs.microsoft.com/en-us/windows/win32/direct3darticles/high-dynamic-range
	const bool bIsHDRCapableDisplayAvailable = checkHDRSupport();
	if (createInfo.HDR)
	{
		if (bIsHDRCapableDisplayAvailable)
		{
			switch (createInfo.bitDepth)
			{
			case SwapChainBitDepth::_10:
				assert(false); // HDR10 isn't supported for now
				break;
			case SwapChainBitDepth::_16:
				// By default, a swap chain created with a floating point pixel format is treated as if it 
				// uses the DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709 color space, which is also known as scRGB.
				SwapChainFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
				m_colorSpace = DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709; // set mColorSpace value to ensure consistant state
				break;
			}
		}
		else
		{
			Warning("No HDR capable display found! Falling back to SDR swapchain.");
			SwapChainFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
			m_colorSpace = DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;
		}
	}

	// https://docs.microsoft.com/en-us/windows/win32/direct3ddxgi/dxgi-flip-model
	// https://docs.microsoft.com/en-us/windows/win32/direct3ddxgi/dxgi-1-4-improvements
	// DXGI_SWAP_EFFECT_FLIP_DISCARD    should be preferred when applications fully render over the backbuffer before 
	//                                  presenting it or are interested in supporting multi-adapter scenarios easily.
	// DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL should be used by applications that rely on partial presentation optimizations 
	//                                  or regularly read from previously presented backbuffers.
	constexpr DXGI_SWAP_EFFECT SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
	swapChainDesc.Width       = createInfo.windowData->width;
	swapChainDesc.Height      = createInfo.windowData->height;
	swapChainDesc.Format      = SwapChainFormat;
	swapChainDesc.Stereo      = FALSE;
	swapChainDesc.SampleDesc  = { 1, 0 };
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.BufferCount = createInfo.numBackBuffers;
	swapChainDesc.Scaling     = DXGI_SCALING_STRETCH;
	swapChainDesc.SwapEffect  = SwapEffect;
	swapChainDesc.AlphaMode   = DXGI_ALPHA_MODE_IGNORE;
	swapChainDesc.Flags = (m_allowTearing && !m_vSync) ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

	DXGI_SWAP_CHAIN_FULLSCREEN_DESC fsSwapChainDesc = {};
	fsSwapChainDesc.Windowed = TRUE;

	ComPtr<IDXGISwapChain1> swapChain1;
	HRESULT result = createInfo.factory->CreateSwapChainForHwnd(m_presentQueue.Get(), createInfo.windowData->hwnd, &swapChainDesc, &fsSwapChainDesc, nullptr, &swapChain1);
	if (FAILED(result))
	{
		Fatal("IDXGIFactory2::CreateSwapChainForHwnd() failed: " + DXErrorToStr(result));
		return false;
	}

	result = createInfo.factory->MakeWindowAssociation(createInfo.windowData->hwnd, DXGI_MWA_NO_ALT_ENTER);
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

	m_format = SwapChainFormat;
	m_currentBackBuffer = m_swapChain->GetCurrentBackBufferIndex();

	// Set color space for HDR if specified
	if (createInfo.HDR && bIsHDRCapableDisplayAvailable)
	{
		constexpr bool bIsOutputSignalST2084 = false; // output signal type for the HDR10 standard
		const bool bIsHDR10Signal = createInfo.bitDepth == SwapChainBitDepth::_10 && bIsOutputSignalST2084;
		EnsureSwapChainColorSpace(createInfo.bitDepth, bIsHDR10Signal);
	}

	// Create Fence & Fence Event
	D3D12_FENCE_FLAGS FenceFlags = D3D12_FENCE_FLAG_NONE;
	m_device->CreateFence(m_fenceValues[m_currentBackBuffer], FenceFlags, IID_PPV_ARGS(&m_fence));
	++m_fenceValues[m_currentBackBuffer];
	m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	if (m_fenceEvent == nullptr)
	{
		Fatal(DXErrorToStr(HRESULT_FROM_WIN32(GetLastError())));
		return false;
	}

	// -- Create the Back Buffers (render target views) Descriptor Heap -- //
	// describe an rtv descriptor heap and create
	D3D12_DESCRIPTOR_HEAP_DESC RTVHeapDesc = {};
	RTVHeapDesc.NumDescriptors = m_numBackBuffers; // number of descriptors for this heap.
	RTVHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV; // this heap is a render target view heap

	// This heap will not be directly referenced by the shaders (not shader visible), as this will store the output from the pipeline otherwise we would set the heap's flag to D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE
	RTVHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	ComPtr<ID3D12DescriptorHeap> rtvDescriptorHeap;
	result = m_device->CreateDescriptorHeap(&RTVHeapDesc, IID_PPV_ARGS(&m_descHeapRTV));
	if (FAILED(result))
	{
		Fatal("ID3D12Device14::CreateDescriptorHeap() failed: " + DXErrorToStr(result));
		return false;
	}
	m_descHeapRTV->SetName(L"SwapChainRTVDescHeap");

	// create RTVs if non-fullscreen swapchain
	if (!createRenderTargetViews())
		return false;

	return true;
}
//=============================================================================
void SwapChainD3D12::Destroy()
{
	// https://docs.microsoft.com/en-us/windows/win32/direct3d12/swap-chains
	// Full-screen swap chains continue to have the restriction that  SetFullscreenState(FALSE, NULL) must be called before the final  release of the swap chain. 
	m_swapChain->SetFullscreenState(FALSE, NULL);

	WaitForGPU();

	m_fence.Reset();
	CloseHandle(m_fenceEvent);

	destroyRenderTargetViews();
	if (m_descHeapRTV) m_descHeapRTV.Reset(); m_descHeapRTV = nullptr;
	if (m_swapChain)   m_swapChain.Reset();   m_swapChain = nullptr;

	m_numBackBuffers = 0;
	m_currentBackBuffer = 0;
	m_numTotalFrames = 0;
	m_vSync = false;
}
//=============================================================================
HRESULT SwapChainD3D12::Resize(int w, int h, DXGI_FORMAT format)
{
	destroyRenderTargetViews();
	for (int i = 0; i < m_numBackBuffers; i++)
	{
		m_fenceValues[i] = m_fenceValues[m_swapChain->GetCurrentBackBufferIndex()];
	}

	HRESULT hr = m_swapChain->ResizeBuffers((UINT)m_numBackBuffers, w, h, format, m_vSync ? 0 : (DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING /*| DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH*/)
	);
	if (hr == S_OK)
	{
		createRenderTargetViews();
	}
	m_currentBackBuffer = m_swapChain->GetCurrentBackBufferIndex();
	m_format = format;
	return hr;
}
//=============================================================================
HRESULT SwapChainD3D12::Present()
{
	constexpr UINT VSYNC_INTERVAL = 1;

	// TODO: glitch detection and avoidance
	// https://docs.microsoft.com/en-us/windows/win32/direct3ddxgi/dxgi-flip-model#avoiding-detecting-and-recovering-from-glitches

	HRESULT hr = {};
	UINT FlagPresent = (!m_vSync && m_allowTearing)
		? DXGI_PRESENT_ALLOW_TEARING // works only in Windowed mode
		: 0;

	if (m_vSync) hr = m_swapChain->Present(VSYNC_INTERVAL, FlagPresent);
	else         hr = m_swapChain->Present(0, FlagPresent);

	if (hr != S_OK)
	{
		// TODO: fatal
	}

	return hr;
}
//=============================================================================
void SwapChainD3D12::MoveToNextFrame()
{
}
//=============================================================================
void SwapChainD3D12::WaitForGPU()
{
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

	//if (mColorSpace != colorSpace)
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
	return DXGI_OUTPUT_DESC1();
}
//=============================================================================
bool SwapChainD3D12::createRenderTargetViews()
{
	return false;
}
//=============================================================================
void SwapChainD3D12::destroyRenderTargetViews()
{
}
//=============================================================================
bool SwapChainD3D12::checkHDRSupport()
{
//#error
	return false;
}
//=============================================================================
#endif // RENDER_D3D12