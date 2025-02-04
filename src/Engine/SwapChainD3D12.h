﻿#pragma once

#if RENDER_D3D12

#include "RHICoreD3D12.h"
#include "HDR.h"
#include "CommandQueueD3D12.h"
#include "FenceD3D12.h"

struct WindowData;
class ContextD3D12;
class DescriptorHeapManagerD3D12;

enum class SwapChainBitDepth
{
	_8 = 0,
	_10,
	_16,
};

struct SwapChainD3D12CreateInfo final
{
	const WindowData&           windowData;
	ContextD3D12&               context;
	DescriptorHeapManagerD3D12& descriptorHeapManager;

	CommandQueueD3D12*          presentQueue{ nullptr };
	int                         numBackBuffers{ MAX_BACK_BUFFER_COUNT };
	bool                        vSync{ false };
	bool                        HDR{ false };
	SwapChainBitDepth           bitDepth = SwapChainBitDepth::_8;
};

class SwapChainD3D12 final
{
public:
	~SwapChainD3D12();

	bool Create(const SwapChainD3D12CreateInfo& createInfo);
	void Destroy();

	bool Resize(uint32_t width, uint32_t height);

	bool Present();
	void MoveToNextFrame();
	void WaitForGPU();

	void SetHDRMetaData(ColorSpace ColorSpace, float MaxOutputNits, float MinOutputNits, float MaxContentLightLevel, float MaxFrameAverageLightLevel);
	void SetHDRMetaData(const SetHDRMetaDataParams& p) { SetHDRMetaData(p.ColorSpace, p.MaxOutputNits, p.MinOutputNits, p.MaxContentLightLevel, p.MaxFrameAverageLightLevel); }
	void ClearHDRMetaData();
	void EnsureSwapChainColorSpace(SwapChainBitDepth swapChainBitDepth, bool bHDR10Signal);
	bool IsInitialized() const { return m_swapChain != nullptr; }

	auto GetNumBackBuffers() const { return m_numBackBuffers; }
	auto GetCurrentBackBufferIndex() const { return m_currentBackBufferIndex; }

	auto GetRenderTargetView() const noexcept
	{
		return m_backBuffersDescriptor[m_currentBackBufferIndex].CPUHandle;
	}

	auto GetDepthStencilView() const noexcept
	{
		return m_depthStencilDescriptor.CPUHandle;
	}

	auto GetCurrentBackBufferRenderTarget() const { return m_backBuffers[m_currentBackBufferIndex].Get(); }
	auto GetNumPresentedFrames() const { return m_numTotalFrames; }
	DXGI_OUTPUT_DESC1 GetContainingMonitorDesc() const;
	auto IsVSyncOn() const { return m_vSync; }
	auto GetFormat() const { return m_backBufferFormat; }
	auto GetDepthStencilFormat() const { return m_depthBufferFormat; }

	auto GetScreenViewport() const noexcept { return m_screenViewport; }
	auto GetScissorRect() const noexcept { return m_scissorRect; }
	auto GetFrameBufferWidth() const noexcept { return m_frameBufferWidth; }
	auto GetFrameBufferHeight() const noexcept { return m_frameBufferHeight; }

	auto IsHDRFormat() const { return m_backBufferFormat == DXGI_FORMAT_R16G16B16A16_FLOAT || m_backBufferFormat == DXGI_FORMAT_R10G10B10A2_UNORM; }
	auto GetColorSpace() const { return m_colorSpace; }
	auto GetHDRMetaData_MaxOutputNits()  const { return static_cast<float>(m_HDRMetaData.MaxMasteringLuminance) / 10000.0f; }
	auto GetHDRMetaData_MinOutputNits() const { return static_cast<float>(m_HDRMetaData.MinMasteringLuminance) / 10000.0f; }

private:
	bool setSize(uint32_t width, uint32_t height);
	bool createSwapChain(const SwapChainD3D12CreateInfo& createInfo);
	bool checkHDRSupport(HWND hwnd, ComPtr<IDXGIFactory7> factory);

	bool createRenderTargetViews();
	bool createDepthStencilViews();
	void destroyRenderTargetViews();
	void destroyDepthStencilViews();

	ComPtr<ID3D12Device14>      m_device;
	ComPtr<D3D12MA::Allocator>  m_allocator;
	CommandQueueD3D12*          m_presentQueue;
	ComPtr<IDXGISwapChain4>     m_swapChain;

	DXGI_FORMAT                 m_backBufferFormat{ DXGI_FORMAT_UNKNOWN };
	const DXGI_FORMAT           m_depthBufferFormat{ DXGI_FORMAT_D32_FLOAT }; // TODO: set?
	DXGI_COLOR_SPACE_TYPE       m_colorSpace{ DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709 }; // Rec709 w/ Gamma2.2
	DXGI_HDR_METADATA_HDR10     m_HDRMetaData;

	D3D12_VIEWPORT              m_screenViewport{};
	D3D12_RECT                  m_scissorRect{};
	uint32_t                    m_frameBufferWidth{ 0 };
	uint32_t                    m_frameBufferHeight{ 0 };

	// Synchronization objects
	FenceD3D12                  m_fence;
	uint64_t                    m_fenceValues[MAX_BACK_BUFFER_COUNT] = {};

	DescriptorHeapManagerD3D12* m_descriptorHeapManager{ nullptr };

	ComPtr<ID3D12Resource>      m_backBuffers[MAX_BACK_BUFFER_COUNT];
	DescriptorHandleD3D12       m_backBuffersDescriptor[MAX_BACK_BUFFER_COUNT];

	ComPtr<ID3D12Resource>      m_depthStencil;
	D3D12MA::Allocation*        m_depthStencilAllocation{ nullptr };
	DescriptorHandleD3D12       m_depthStencilDescriptor{};

	unsigned short              m_numBackBuffers{ 0 };
	unsigned short              m_currentBackBufferIndex{ 0 };
	unsigned long long          m_numTotalFrames{ 0 };
	bool                        m_vSync{ false };
	bool                        m_supportAllowTearing{ false };
};

#endif // RENDER_D3D12