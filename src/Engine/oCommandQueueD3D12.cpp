#include "stdafx.h"
#if RENDER_D3D12
#include "oCommandQueueD3D12.h"
#include "oRenderCoreD3D12.h"
#include "Log.h"
//=============================================================================
CommandQueueD3D12::CommandQueueD3D12(ComPtr<ID3D12Device> device, D3D12_COMMAND_LIST_TYPE commandType)
	: m_queueType(commandType)
{
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Type                     = m_queueType;
	queueDesc.Priority                 = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	queueDesc.Flags                    = D3D12_COMMAND_QUEUE_FLAG_NONE;
	queueDesc.NodeMask                 = 0;
	HRESULT result = device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_queue));
	if (FAILED(result))
	{
		Fatal("ID3D12Device::CreateCommandQueue() failed: " + DXErrorToStr(result));
		return;
	}

	result = device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence));
	if (FAILED(result))
	{
		Fatal("ID3D12Device::CreateFence() failed: " + DXErrorToStr(result));
		return;
	}

	result = m_fence->Signal(m_lastCompletedFenceValue);
	if (FAILED(result))
	{
		Fatal("ID3D12Fence::Signal() failed: " + DXErrorToStr(result));
		return;
	}

	m_fenceEventHandle = CreateEventEx(nullptr, nullptr, 0, EVENT_ALL_ACCESS);
	if (!m_fenceEventHandle)
	{
		Fatal("CreateEventEx() failed.");
		return;
	}
}
//=============================================================================
CommandQueueD3D12::~CommandQueueD3D12()
{
	CloseHandle(m_fenceEventHandle);
}
//=============================================================================
uint64_t CommandQueueD3D12::PollCurrentFenceValue()
{
	m_lastCompletedFenceValue = (std::max)(m_lastCompletedFenceValue, m_fence->GetCompletedValue());
	return m_lastCompletedFenceValue;
}
//=============================================================================
bool CommandQueueD3D12::IsFenceComplete(uint64_t fenceValue)
{
	if (fenceValue > m_lastCompletedFenceValue)
	{
		PollCurrentFenceValue();
	}

	return fenceValue <= m_lastCompletedFenceValue;
}
//=============================================================================
void CommandQueueD3D12::InsertWait(uint64_t fenceValue)
{
	m_queue->Wait(m_fence.Get(), fenceValue);
}
//=============================================================================
void CommandQueueD3D12::InsertWaitForQueueFence(CommandQueueD3D12* otherQueue, uint64_t fenceValue)
{
	m_queue->Wait(otherQueue->GetFence().Get(), fenceValue);
}
//=============================================================================
void CommandQueueD3D12::InsertWaitForQueue(CommandQueueD3D12* otherQueue)
{
	m_queue->Wait(otherQueue->GetFence().Get(), otherQueue->GetNextFenceValue() - 1);
}
//=============================================================================
void CommandQueueD3D12::WaitForFenceCPUBlocking(uint64_t fenceValue)
{
	if (IsFenceComplete(fenceValue)) return;

	{
		std::lock_guard<std::mutex> lockGuard(m_eventMutex);

		m_fence->SetEventOnCompletion(fenceValue, m_fenceEventHandle);
		WaitForSingleObjectEx(m_fenceEventHandle, INFINITE, false);
		m_lastCompletedFenceValue = fenceValue;
	}
}
//=============================================================================
void CommandQueueD3D12::WaitForIdle()
{
	WaitForFenceCPUBlocking(m_nextFenceValue - 1);
}
//=============================================================================
uint64_t CommandQueueD3D12::ExecuteCommandList(ID3D12CommandList* commandList)
{
	HRESULT result = static_cast<ID3D12GraphicsCommandList*>(commandList)->Close();
	if (FAILED(result))
	{
		Fatal("ID3D12CommandList::Close() failed: " + DXErrorToStr(result));
		return 0;
	}

	m_queue->ExecuteCommandLists(1, &commandList);

	return SignalFence();
}
//=============================================================================
uint64_t CommandQueueD3D12::SignalFence()
{
	std::lock_guard<std::mutex> lockGuard(m_fenceMutex);

	m_queue->Signal(m_fence.Get(), m_nextFenceValue);

	return m_nextFenceValue++;
}
//=============================================================================

#endif // RENDER_D3D12