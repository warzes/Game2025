#pragma once

#if RENDER_D3D12

class oCommandQueueD3D12 final
{
public:
	oCommandQueueD3D12(ComPtr<ID3D12Device> device, D3D12_COMMAND_LIST_TYPE commandType);
	~oCommandQueueD3D12();

	bool IsFenceComplete(uint64_t fenceValue);
	void InsertWait(uint64_t fenceValue);
	void InsertWaitForQueueFence(oCommandQueueD3D12* otherQueue, uint64_t fenceValue);
	void InsertWaitForQueue(oCommandQueueD3D12* otherQueue);
	void WaitForFenceCPUBlocking(uint64_t fenceValue);
	void WaitForIdle();

	uint64_t PollCurrentFenceValue();
	uint64_t GetLastCompletedFence() const { return m_lastCompletedFenceValue; }
	uint64_t GetNextFenceValue() const { return m_nextFenceValue; }
	uint64_t ExecuteCommandList(ID3D12CommandList* commandList);
	uint64_t SignalFence();

	auto GetDeviceQueue() { return m_queue; }
	auto GetFence() { return m_fence; }

private:
	D3D12_COMMAND_LIST_TYPE    m_queueType{ D3D12_COMMAND_LIST_TYPE_DIRECT };
	ComPtr<ID3D12CommandQueue> m_queue{ nullptr };
	ComPtr<ID3D12Fence>        m_fence{ nullptr };
	uint64_t                   m_nextFenceValue{ 1 };
	uint64_t                   m_lastCompletedFenceValue{ 0 };
	HANDLE                     m_fenceEventHandle{ 0 };
	std::mutex                 m_fenceMutex;
	std::mutex                 m_eventMutex;
};

#endif // RENDER_D3D12