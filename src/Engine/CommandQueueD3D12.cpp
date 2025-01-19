#include "stdafx.h"
#if RENDER_D3D12
#include "CommandQueueD3D12.h"
#include "Log.h"
#include "FenceD3D12.h"
#include "HelperD3D12.h"
//=============================================================================
bool CommandQueueD3D12::Create(ID3D12Device14* device, D3D12_COMMAND_LIST_TYPE type, const char* name)
{
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Type = type;
	queueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	queueDesc.NodeMask = 0;
	HRESULT result = device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_queue));
	if (FAILED(result))
	{
		Fatal("ID3D12Device14::CreateCommandQueue() failed: " + DXErrorToStr(result));
		return false;
	}

	if (name)
		SetDebugObjectName(m_queue.Get(), name);

	return true;
}
//=============================================================================
void CommandQueueD3D12::Destroy()
{
	m_queue.Reset();
}
//=============================================================================
bool CommandQueueD3D12::Signal(const FenceD3D12& fence, uint64_t FenceWaitValue)
{
	HRESULT result = m_queue->Signal(fence, FenceWaitValue);
	if (FAILED(result))
	{
		Fatal("ID3D12CommandQueue::Signal() failed: " + DXErrorToStr(result));
		return false;
	}
	return true;
}
//=============================================================================
void CommandQueueD3D12::ExecuteCommandList(ComPtr<ID3D12GraphicsCommandList10> commandList)
{
	m_queue->ExecuteCommandLists(1, CommandListCast(commandList.GetAddressOf()));
}
//=============================================================================
#endif // RENDER_D3D12