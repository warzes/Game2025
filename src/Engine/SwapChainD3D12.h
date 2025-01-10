﻿#pragma once

#if RENDER_D3D12

#include "RHICoreD3D12.h"
#include "DescriptorHeapD3D12.h"
#include "HDR.h"

struct WindowData;

enum class SwapChainBitDepth
{
	_8 = 0,
	_10,
	_16,
};

struct SwapChainD3D12CreateInfo final
{
	WindowData*                windowData{ nullptr };
	ComPtr<IDXGIFactory7>      factory;
	ComPtr<ID3D12Device14>     device;
	ComPtr<ID3D12CommandQueue> cmdQueue;
	int                        numBackBuffers{ MAX_BACK_BUFFER_COUNT };
	bool                       allowTearing{ true };
	bool                       vSync{ false };
	bool                       HDR{ false };
	SwapChainBitDepth          bitDepth = SwapChainBitDepth::_8;
};

class SwapChainD3D12 final
{
public:
	~SwapChainD3D12();

	bool Create(const SwapChainD3D12CreateInfo& createInfo);
	void Destroy();

	HRESULT Resize(int w, int h, DXGI_FORMAT format);

	HRESULT Present();
	void MoveToNextFrame();
	void WaitForGPU();

	void SetHDRMetaData(ColorSpace ColorSpace, float MaxOutputNits, float MinOutputNits, float MaxContentLightLevel, float MaxFrameAverageLightLevel);
	void SetHDRMetaData(const SetHDRMetaDataParams& p) { SetHDRMetaData(p.ColorSpace, p.MaxOutputNits, p.MinOutputNits, p.MaxContentLightLevel, p.MaxFrameAverageLightLevel); }
	void ClearHDRMetaData();
	void EnsureSwapChainColorSpace(SwapChainBitDepth swapChainBitDepth, bool bHDR10Signal);
	bool IsInitialized() const { return this->m_swapChain != nullptr; }

	auto GetNumBackBuffers() const { return m_numBackBuffers; }
	auto GetCurrentBackBufferIndex() const { return m_currentBackBuffer; }
	auto GetCurrentBackBufferRTVHandle() const { return CD3DX12_CPU_DESCRIPTOR_HANDLE(m_descHeapRTV->GetCPUDescriptorHandleForHeapStart(), GetCurrentBackBufferIndex(), m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV)); }
	auto GetCurrentBackBufferRenderTarget() const { return m_renderTargets[GetCurrentBackBufferIndex()].Get(); }
	auto GetNumPresentedFrames() const { return m_numTotalFrames; }
	DXGI_OUTPUT_DESC1 GetContainingMonitorDesc() const;
	auto IsVSyncOn() const { return m_vSync; }
	auto GetFormat() const { return m_format; }

	auto IsHDRFormat() const { return m_format == DXGI_FORMAT_R16G16B16A16_FLOAT || m_format == DXGI_FORMAT_R10G10B10A2_UNORM; }
	auto GetColorSpace() const { return m_colorSpace; }
	auto GetHDRMetaData_MaxOutputNits()  const { return static_cast<float>(m_HDRMetaData.MaxMasteringLuminance) / 10000.0f; }
	auto GetHDRMetaData_MinOutputNits() const { return static_cast<float>(m_HDRMetaData.MinMasteringLuminance) / 10000.0f; }

private:
	bool checkHDRSupport();

	bool createRenderTargetViews();
	void destroyRenderTargetViews();

	ComPtr<ID3D12Device14>     m_device;
	ComPtr<ID3D12CommandQueue> m_presentQueue;
	ComPtr<IDXGISwapChain4>    m_swapChain;

	DXGI_FORMAT                m_format{ DXGI_FORMAT_UNKNOWN };
	DXGI_COLOR_SPACE_TYPE      m_colorSpace{ DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709 }; // Rec709 w/ Gamma2.2
	DXGI_HDR_METADATA_HDR10    m_HDRMetaData;

	// Synchronization objects
	ComPtr<ID3D12Fence>             m_fence;
	uint64_t                        m_fenceValues[MAX_BACK_BUFFER_COUNT] = {};
	//Microsoft::WRL::Wrappers::Event m_fenceEvent;
	HANDLE m_fenceEvent;

	ComPtr<ID3D12Resource>              m_renderTargets[MAX_BACK_BUFFER_COUNT];
	//StagingDescriptorHeapD3D12*         RTVStagingDescriptorHeap{ nullptr };
	ComPtr<ID3D12DescriptorHeap>        m_descHeapRTV;

	unsigned short     m_numBackBuffers{ 0 };
	unsigned short     m_currentBackBuffer{ 0 };
	unsigned long long m_numTotalFrames{ 0 };
	bool               m_vSync{ false };
	bool               m_allowTearing{ false };


};

#endif // RENDER_D3D12