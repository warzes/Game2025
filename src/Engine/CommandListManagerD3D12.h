#pragma once

#if RENDER_D3D12

#include "CommandQueueD3D12.h"

class CommandListManagerD3D12 final
{
	friend class CommandContextD3D12;

public:
	CommandListManagerD3D12();
	~CommandListManagerD3D12();

	void Create(ID3D12Device14* device);
	void Shutdown();

	CommandQueueD3D12& GetGraphicsQueue() { return m_graphicsQueue; }
	CommandQueueD3D12& GetComputeQueue() { return m_computeQueue; }
	CommandQueueD3D12& GetCopyQueue() { return m_copyQueue; }

	CommandQueueD3D12& GetQueue(D3D12_COMMAND_LIST_TYPE Type = D3D12_COMMAND_LIST_TYPE_DIRECT)
	{
		switch (Type)
		{
		case D3D12_COMMAND_LIST_TYPE_COMPUTE: return m_computeQueue;
		case D3D12_COMMAND_LIST_TYPE_COPY: return m_copyQueue;
		default: return m_graphicsQueue;
		}
	}

	ID3D12CommandQueue* GetCommandQueue()
	{
		return m_graphicsQueue.GetCommandQueue();
	}

	void CreateNewCommandList(D3D12_COMMAND_LIST_TYPE type, ID3D12GraphicsCommandList** list, ID3D12CommandAllocator** allocator);

	// Test to see if a fence has already been reached
	bool IsFenceComplete(uint64_t fenceValue)
	{
		return GetQueue(D3D12_COMMAND_LIST_TYPE(fenceValue >> 56)).IsFenceComplete(fenceValue);
	}

	// The CPU will wait for a fence to reach a specified value
	void WaitForFence(uint64_t fenceValue);

	// The CPU will wait for all command queues to empty (so that the GPU is idle)
	void IdleGPU()
	{
		m_graphicsQueue.WaitForIdle();
		m_computeQueue.WaitForIdle();
		m_copyQueue.WaitForIdle();
	}

private:
	ID3D12Device14*   m_device{ nullptr };
	CommandQueueD3D12 m_graphicsQueue;
	CommandQueueD3D12 m_computeQueue;
	CommandQueueD3D12 m_copyQueue;
};

extern CommandListManagerD3D12 gCommandManager;

#endif // RENDER_D3D12