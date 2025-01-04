#pragma once

#if RENDER_D3D12

#include "CommandAllocatorPoolD3D12.h"

class CommandQueueD3D12 final
{
	friend class CommandListManagerD3D12;
	friend class CommandContextD3D12;

public:
	CommandQueueD3D12(D3D12_COMMAND_LIST_TYPE type);
	~CommandQueueD3D12();

	void Create(ID3D12Device14* device);
	void Shutdown();

	inline bool IsReady()
	{
		return m_commandQueue != nullptr;
	}

	uint64_t IncrementFence();
	bool IsFenceComplete(uint64_t fenceValue);
	void StallForFence(uint64_t fenceValue);
	void StallForProducer(CommandQueueD3D12& producer);
	void WaitForFence(uint64_t fenceValue);
	void WaitForIdle() { WaitForFence(IncrementFence()); }

	ID3D12CommandQueue* GetCommandQueue() { return m_commandQueue; }
	uint64_t GetNextFenceValue() { return m_nextFenceValue; }

private:
	uint64_t executeCommandList(ID3D12CommandList* list);
	ID3D12CommandAllocator* requestAllocator();
	void discardAllocator(uint64_t fenceValueForReset, ID3D12CommandAllocator* allocator);

	ID3D12CommandQueue*           m_commandQueue{ nullptr };

	const D3D12_COMMAND_LIST_TYPE m_type;

	CommandAllocatorPoolD3D12     m_allocatorPool;
	std::mutex                    m_fenceMutex;
	std::mutex                    m_eventMutex;

	// Lifetime of these objects is managed by the descriptor cache
	ID3D12Fence*                  m_fence{ nullptr };
	uint64_t                      m_nextFenceValue;
	uint64_t                      m_lastCompletedFenceValue;
	HANDLE                        m_fenceEventHandle;
};

#endif // RENDER_D3D12