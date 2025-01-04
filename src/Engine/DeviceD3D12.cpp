#include "stdafx.h"
#if RENDER_D3D12
#include "DeviceD3D12.h"
#include "Log.h"
#include "RHICoreD3D12.h"
//=============================================================================
#if RHI_VALIDATION_ENABLED
void D3D12MessageCallbackFunc(D3D12_MESSAGE_CATEGORY Category, D3D12_MESSAGE_SEVERITY Severity, D3D12_MESSAGE_ID ID, LPCSTR pDescription, void* pContext) noexcept
{
	Print(pDescription);
}
#endif // RHI_VALIDATION_ENABLED
//=============================================================================
DeviceD3D12::~DeviceD3D12()
{
	assert(!m_device);
	assert(!m_allocator);
}
//=============================================================================
bool DeviceD3D12::Create(ComPtr<IDXGIAdapter4> adapter)
{
	if (!createDevice(adapter)) return false;
	if (!createAllocator(adapter)) return false;

	return true;
}
//=============================================================================
void DeviceD3D12::Destroy()
{
	m_allocator.Reset();
	m_device.Reset();
}
//=============================================================================
bool DeviceD3D12::createDevice(ComPtr<IDXGIAdapter4> adapter)
{
	const D3D_FEATURE_LEVEL featureLevels[] = {
				D3D_FEATURE_LEVEL_12_2,
				D3D_FEATURE_LEVEL_12_1,
				D3D_FEATURE_LEVEL_12_0,
				D3D_FEATURE_LEVEL_11_1,
				D3D_FEATURE_LEVEL_11_0,
	};
	HRESULT result{};
	for (auto& featureLevel : featureLevels)
	{

		result = D3D12CreateDevice(adapter.Get(), featureLevel, IID_PPV_ARGS(&m_device));
		if (SUCCEEDED(result))
		{
			Print("Direct3D 12 Feature Level: " + ConvertToStr(featureLevel));
			break;
		}
	}
	if (FAILED(result))
	{
		Fatal("D3D12CreateDevice() failed: " + DXErrorToStr(result));
		return false;
	}

	m_device->SetName(L"MiniEngine");

	result = m_d3d12Features.Init(m_device.Get());
	if (FAILED(result))
	{
		Fatal("CD3DX12FeatureSupport::Init() failed: " + DXErrorToStr(result));
		return false;
	}

#if RHI_VALIDATION_ENABLED
	ComPtr<ID3D12InfoQueue1> d3dInfoQueue;
	if (SUCCEEDED(m_device.As(&d3dInfoQueue)))
	{
		d3dInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
		d3dInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
		//d3dInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);

		D3D12_MESSAGE_ID filteredMessages[] = {
			//D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,
			//D3D12_MESSAGE_ID_CLEARDEPTHSTENCILVIEW_MISMATCHINGCLEARVALUE,
			D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE, // This warning occurs when using capture frame while graphics debugging.
			D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE,
			D3D12_MESSAGE_ID_EXECUTECOMMANDLISTS_WRONGSWAPCHAINBUFFERREFERENCE,
			D3D12_MESSAGE_ID_RESOURCE_BARRIER_MISMATCHING_COMMAND_LIST_TYPE
		};
		D3D12_INFO_QUEUE_FILTER filter = {};
		filter.DenyList.NumIDs = static_cast<UINT>(std::size(filteredMessages));
		filter.DenyList.pIDList = filteredMessages;
		d3dInfoQueue->AddStorageFilterEntries(&filter);

		d3dInfoQueue->RegisterMessageCallback(D3D12MessageCallbackFunc, D3D12_MESSAGE_CALLBACK_FLAG_NONE, nullptr, nullptr);
	}
#endif // RHI_VALIDATION_ENABLED

	return true;
}
//=============================================================================
bool DeviceD3D12::createAllocator(ComPtr<IDXGIAdapter4> adapter)
{
	D3D12MA::ALLOCATOR_DESC desc = {};
	desc.Flags = D3D12MA::ALLOCATOR_FLAG_NONE;
	desc.pDevice = m_device.Get();
	desc.pAdapter = adapter.Get();

	HRESULT result = D3D12MA::CreateAllocator(&desc, &m_allocator);
	if (FAILED(result))
	{
		Fatal("CreateAllocator() failed: " + DXErrorToStr(result));
		return false;
	}

	return true;
}
//=============================================================================
#endif // RENDER_D3D12