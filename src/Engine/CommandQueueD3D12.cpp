#include "stdafx.h"
#if RENDER_D3D12
#include "CommandQueueD3D12.h"
#include "RHICoreD3D12.h"
#include "Log.h"
#include "CommandListManagerD3D12.h" // TODO:
//=============================================================================
CommandQueueD3D12::CommandQueueD3D12(D3D12_COMMAND_LIST_TYPE type)
	: m_type(type)
	, m_nextFenceValue((uint64_t)type << 56 | 1)
	, m_lastCompletedFenceValue((uint64_t)type << 56)
	, m_allocatorPool(type)
{
}
//=============================================================================
CommandQueueD3D12::~CommandQueueD3D12()
{
	Shutdown();
}
//=============================================================================
void CommandQueueD3D12::Create(ID3D12Device14* device)
{
	assert(device != nullptr);
	assert(!IsReady());
	assert(m_allocatorPool.Size() == 0);

	D3D12_COMMAND_QUEUE_DESC QueueDesc = {};
	QueueDesc.Type                     = m_type;
	QueueDesc.NodeMask                 = 1;
	HRESULT result = device->CreateCommandQueue(&QueueDesc, IID_PPV_ARGS(&m_commandQueue));
	if (FAILED(result))
	{
		Fatal("ID3D12Device14::CreateCommandQueue() failed: " + DXErrorToStr(result));
		return;
	}
	m_commandQueue->SetName(L"CommandListManager::m_commandQueue");

	result = device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence));
	if (FAILED(result))
	{
		Fatal("ID3D12Device14::CreateFence() failed: " + DXErrorToStr(result));
		return;
	}
	m_fence->SetName(L"CommandListManager::m_fence");
	m_fence->Signal((uint64_t)m_type << 56);

	m_fenceEventHandle = CreateEvent(nullptr, false, false, nullptr);
	assert(m_fenceEventHandle != nullptr);

	m_allocatorPool.Create(device);

	assert(IsReady());
}
//=============================================================================
void CommandQueueD3D12::Shutdown()
{
	if (m_commandQueue == nullptr) return;

	m_allocatorPool.Shutdown();

	CloseHandle(m_fenceEventHandle);

	m_fence->Release();
	m_fence = nullptr;

	m_commandQueue->Release();
	m_commandQueue = nullptr;
}
//=============================================================================
uint64_t CommandQueueD3D12::IncrementFence()
{
	std::lock_guard<std::mutex> LockGuard(m_fenceMutex);
	m_commandQueue->Signal(m_fence, m_nextFenceValue);
	return m_nextFenceValue++;
}
//=============================================================================
bool CommandQueueD3D12::IsFenceComplete(uint64_t fenceValue)
{
	// Avoid querying the fence value by testing against the last one seen.
	// The max() is to protect against an unlikely race condition that could cause the last
	// completed fence value to regress.
	if (fenceValue > m_lastCompletedFenceValue)
		m_lastCompletedFenceValue = std::max(m_lastCompletedFenceValue, m_fence->GetCompletedValue());

	return fenceValue <= m_lastCompletedFenceValue;
}
//=============================================================================
void CommandQueueD3D12::StallForFence(uint64_t fenceValue)
{
	CommandQueueD3D12& producer = gCommandManager.GetQueue((D3D12_COMMAND_LIST_TYPE)(fenceValue >> 56)); // TODO: возможно избавиться
	m_commandQueue->Wait(producer.m_fence, fenceValue);
}
//=============================================================================
void CommandQueueD3D12::StallForProducer(CommandQueueD3D12& producer)
{
	assert(producer.m_nextFenceValue > 0);
	m_commandQueue->Wait(producer.m_fence, producer.m_nextFenceValue - 1);
}
//=============================================================================
void CommandQueueD3D12::WaitForFence(uint64_t fenceValue)
{
	if (IsFenceComplete(fenceValue))
		return;

	// TODO:  Think about how this might affect a multi-threaded situation.  Suppose thread A
	// wants to wait for fence 100, then thread B comes along and wants to wait for 99.  If
	// the fence can only have one event set on completion, then thread B has to wait for 
	// 100 before it knows 99 is ready.  Maybe insert sequential events?
	{
		std::lock_guard<std::mutex> LockGuard(m_eventMutex);

		m_fence->SetEventOnCompletion(fenceValue, m_fenceEventHandle);
		WaitForSingleObject(m_fenceEventHandle, INFINITE);
		m_lastCompletedFenceValue = fenceValue;
	}
}
//=============================================================================
uint64_t CommandQueueD3D12::executeCommandList(ID3D12CommandList* list)
{
	std::lock_guard<std::mutex> LockGuard(m_fenceMutex);

	HRESULT result = ((ID3D12GraphicsCommandList*)list)->Close();
	if (FAILED(result))
	{
		Fatal("ID3D12GraphicsCommandList::Close() failed: " + DXErrorToStr(result));
		return 0;
	}

	// Kickoff the command list
	m_commandQueue->ExecuteCommandLists(1, &list);

	// Signal the next fence value (with the GPU)
	m_commandQueue->Signal(m_fence, m_nextFenceValue);

	// And increment the fence value.  
	return m_nextFenceValue++;
}
//=============================================================================
ID3D12CommandAllocator* CommandQueueD3D12::requestAllocator()
{
	uint64_t completedFence = m_fence->GetCompletedValue();
	return m_allocatorPool.RequestAllocator(completedFence);
}
//=============================================================================
void CommandQueueD3D12::discardAllocator(uint64_t fenceValue, ID3D12CommandAllocator* allocator)
{
	m_allocatorPool.DiscardAllocator(fenceValue, allocator);
}
//=============================================================================
#endif // RENDER_D3D12