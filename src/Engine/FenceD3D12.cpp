#include "stdafx.h"
#if RENDER_D3D12
#include "FenceD3D12.h"
#include "Log.h"
#include "RHICoreD3D12.h"
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
	{
		result = SetName(m_fence.Get(), debugName);
		if (FAILED(result))
		{
			Fatal("SetName() failed: " + DXErrorToStr(result));
			return false;
		}
	}
	m_event.Attach(CreateEventEx(nullptr, nullptr, 0, EVENT_MODIFY_STATE | SYNCHRONIZE));
	if (!m_event.IsValid())
	{
		Fatal("CreateEventEx() failed.");
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
bool FenceD3D12::WaitOnCPU(uint64_t FenceWaitValue) const
{
	// If the next frame is not ready to be rendered yet, wait until it is ready.
	if (m_fence->GetCompletedValue() < FenceWaitValue)
	{
		HRESULT result = m_fence->SetEventOnCompletion(FenceWaitValue, m_event.Get());
		if (FAILED(result))
		{
			Fatal("ID3D12Fence::SetEventOnCompletion() failed: " + DXErrorToStr(result));
			return false;
		}

		std::ignore = WaitForSingleObjectEx(m_event.Get(), INFINITE, FALSE);
	}
	return true;
}
//=============================================================================
#endif // RENDER_D3D12