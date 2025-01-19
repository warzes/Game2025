#include "stdafx.h"
#if RENDER_D3D12
#include "FenceD3D12.h"
#include "Log.h"
#include "HelperD3D12.h"
#include "GPUMarker.h"
//=============================================================================
bool FenceD3D12::Create(ID3D12Device14* device, const char* debugName)
{
	HRESULT result = device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence));
	if (FAILED(result))
	{
		Fatal("ID3D12Device14::CreateFence() failed: " + DXErrorToStr(result));
		return false;
	}

	if (debugName)
		SetDebugObjectName(m_fence.Get(), debugName);

	m_event.Attach(CreateEventEx(nullptr, nullptr, 0, EVENT_MODIFY_STATE | SYNCHRONIZE));
	if (!m_event.IsValid())
	{
		Fatal("CreateEventEx() failed: " + DXErrorToStr(HRESULT_FROM_WIN32(GetLastError())));
		return false;
	}

	return true;
}
//=============================================================================
void FenceD3D12::Destroy()
{
	m_fence.Reset();
	m_event.Close();
}
//=============================================================================
bool FenceD3D12::IsFenceComplete(uint64_t fenceValue) const
{
	return m_fence->GetCompletedValue() >= fenceValue;
}
//=============================================================================
bool FenceD3D12::WaitOnCPU(uint64_t FenceWaitValue, DWORD timeout) const
{
	if (timeout == 0) return false;
	if (!IsFenceComplete(FenceWaitValue))
	{
		SCOPED_CPU_MARKER_C("GPU_BOUND", 0xFF005500);
		HRESULT result = m_fence->SetEventOnCompletion(FenceWaitValue, m_event.Get());
		if (FAILED(result))
		{
			Fatal("ID3D12Fence::SetEventOnCompletion() failed: " + DXErrorToStr(result));
			return false;
		}

		DWORD ret = WaitForSingleObjectEx(m_event.Get(), timeout, FALSE);
		if (ret == WAIT_OBJECT_0)
			return true;
		if (ret == WAIT_TIMEOUT)
		{
			Warning("SwapChain timed out on WaitForGPU(): Signal=" + std::to_string(FenceWaitValue));
		}
	}
	return true;
}
//=============================================================================
#endif // RENDER_D3D12