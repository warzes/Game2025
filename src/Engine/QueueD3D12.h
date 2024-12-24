﻿#pragma once

#if RENDER_D3D12

class QueueD3D12 final
{
public:
	QueueD3D12(ID3D12Device* device, D3D12_COMMAND_LIST_TYPE commandType);
	~QueueD3D12();

	bool IsFenceComplete(uint64_t fenceValue);
	void InsertWait(uint64_t fenceValue);
	void InsertWaitForQueueFence(QueueD3D12* otherQueue, uint64_t fenceValue);
	void InsertWaitForQueue(QueueD3D12* otherQueue);
	void WaitForFenceCPUBlocking(uint64_t fenceValue);
	void WaitForIdle();

	uint64_t PollCurrentFenceValue();
	uint64_t GetLastCompletedFence() { return m_lastCompletedFenceValue; }
	uint64_t GetNextFenceValue() { return m_nextFenceValue; }
	uint64_t ExecuteCommandList(ID3D12CommandList* commandList);
	uint64_t SignalFence();

	auto GetDeviceQueue() { return m_queue; }
	auto GetFence() { return m_fence; }

private:
	D3D12_COMMAND_LIST_TYPE m_queueType{ D3D12_COMMAND_LIST_TYPE_DIRECT };
	ID3D12CommandQueue*     m_queue{ nullptr };
	ID3D12Fence*            m_fence{ nullptr };
	uint64_t                m_nextFenceValue{ 1 };
	uint64_t                m_lastCompletedFenceValue{ 0 };
	HANDLE                  m_fenceEventHandle{ 0 };
	std::mutex              m_fenceMutex;
	std::mutex              m_eventMutex;
};

#endif // RENDER_D3D12