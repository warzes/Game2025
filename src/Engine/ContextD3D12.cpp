#include "stdafx.h"
#if RENDER_D3D12
#include "ContextD3D12.h"
#include "Log.h"
#include "RenderCore.h"
#include "RHICoreD3D12.h"
//=============================================================================
ContextD3D12::~ContextD3D12()
{
	assert(!m_adapter);
	assert(!m_factory);
}
//=============================================================================
bool ContextD3D12::Create(const ContextCreateInfo& createInfo)
{
	enableDebugLayer(createInfo);
	if (!createFactory()) return false;
	if (!selectAdapter(createInfo.useWarp)) return false;
	if (!m_device.Create(m_adapter)) return false;

	return true;
}
//=============================================================================
void ContextD3D12::Destroy()
{
	m_device.Destroy();
	m_adapter.Reset();
	m_factory.Reset();

#if defined(_DEBUG)
	ComPtr<IDXGIDebug1> debug;
	if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&debug))))
	{
		debug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_FLAGS(DXGI_DEBUG_RLO_SUMMARY | DXGI_DEBUG_RLO_DETAIL | DXGI_DEBUG_RLO_IGNORE_INTERNAL));
	}
#endif
}
//=============================================================================
void ContextD3D12::enableDebugLayer(const ContextCreateInfo& createInfo)
{
#if RHI_VALIDATION_ENABLED
	ComPtr<IDXGIInfoQueue> dxgiInfoQueue;
	if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiInfoQueue))))
	{
		dxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR, true);
		dxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_CORRUPTION, true);

		DXGI_INFO_QUEUE_MESSAGE_ID hide[] =
		{
			80 /* IDXGISwapChain::GetContainingOutput: The swapchain's adapter does not control the output on which the swapchain's window resides. */,
		};
		DXGI_INFO_QUEUE_FILTER filter = {};
		filter.DenyList.NumIDs = static_cast<UINT>(std::size(hide));
		filter.DenyList.pIDList = hide;
		dxgiInfoQueue->AddStorageFilterEntries(DXGI_DEBUG_DXGI, &filter);
	}

	ComPtr<ID3D12Debug> debugController;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
	{
		debugController->EnableDebugLayer();
		Print("Direct3D 12 Enable Debug Layer");
	}
	else
	{
		Warning("Direct3D Debug Device is not available");
		return;
	}

	ComPtr<ID3D12Debug1> debugController1;
	if (FAILED(debugController.As(&debugController1))) return;
	if (createInfo.debug.enableGPUBasedValidation)
	{
		debugController1->SetEnableGPUBasedValidation(TRUE);
		Print("Direct3D 12 Enable Debug GPU Based Validation");
	}
	if (createInfo.debug.enableSynchronizedCommandQueueValidation)
	{
		debugController1->SetEnableSynchronizedCommandQueueValidation(TRUE);
		Print("Direct3D 12 Enable Debug Synchronized Command Queue Validation");
	}

	ComPtr<ID3D12Debug5> debugController5;
	if (FAILED(debugController1.As(&debugController5))) return;
	if (createInfo.debug.enableAutoName)
	{
		debugController5->SetEnableAutoName(TRUE);
		Print("Direct3D 12 Enable Debug Auto Name");
	}
#endif // RHI_VALIDATION_ENABLED
}
//=============================================================================
bool ContextD3D12::createFactory()
{
	UINT dxgiFactoryFlags = 0;
#if RHI_VALIDATION_ENABLED
	dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
#endif // RHI_VALIDATION_ENABLED

	ComPtr<IDXGIFactory> DXGIFactory;
	HRESULT result = CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&DXGIFactory));
	if (FAILED(result))
	{
		Fatal("CreateDXGIFactory2() failed: " + DXErrorToStr(result));
		return false;
	}
	result = DXGIFactory.As(&m_factory);
	if (FAILED(result))
	{
		Fatal("IDXGIFactory As IDXGIFactory7 failed: " + DXErrorToStr(result));
		return false;
	}

	{
		// при отключении vsync на экране могут быть разрывы
		BOOL allowTearing = FALSE;
		if (FAILED(m_factory->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allowTearing, sizeof(allowTearing))))
			m_supportAllowTearing = false;
		else
			m_supportAllowTearing = (allowTearing == TRUE);
	}

	return true;
}
//=============================================================================
bool ContextD3D12::selectAdapter(bool useWarp)
{
	HRESULT result{};
	ComPtr<IDXGIAdapter> DXGIAdapter;
	if (useWarp)
	{
		result = m_factory->EnumWarpAdapter(IID_PPV_ARGS(&DXGIAdapter));
		if (FAILED(result))
		{
			Fatal("IDXGIFactory7::EnumWarpAdapter() failed: " + DXErrorToStr(result));
			return false;
		}
	}
	else
	{
		auto NextAdapted = [&](uint32_t adapterIndex, ComPtr<IDXGIAdapter1>& adapter) {
			return m_factory->EnumAdapterByGpuPreference(adapterIndex, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&adapter));
			};

		ComPtr<IDXGIAdapter1> adapter;
		for (uint32_t adapterIndex = 0; DXGI_ERROR_NOT_FOUND != NextAdapted(adapterIndex, adapter); ++adapterIndex)
		{
			DXGI_ADAPTER_DESC1 desc = {};
			adapter->GetDesc1(&desc);
			if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
			{
				DXGIAdapter.Reset();
				continue;
			}

			DXGIAdapter = adapter;
			break;
		}

		if (!DXGIAdapter)
		{
			Fatal("IDXGIFactory7::EnumAdapterByGpuPreference() failed: " + DXErrorToStr(result));
			return false;
		}
	}
	result = DXGIAdapter.As(&m_adapter);
	if (FAILED(result))
	{
		Fatal("DXGIAdapter As DXGIAdapter4 failed: " + DXErrorToStr(result));
		return false;
	}

	return true;
}
//=============================================================================
#endif // RENDER_D3D12