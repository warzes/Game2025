#pragma once

#if RENDER_D3D12

class CommandAllocatorPoolD3D12 final
{
public:
	CommandAllocatorPoolD3D12(D3D12_COMMAND_LIST_TYPE type);
	~CommandAllocatorPoolD3D12();

	void Create(ID3D12Device14* device);
	void Shutdown();

	ID3D12CommandAllocator* RequestAllocator(uint64_t completedFenceValue);
	void DiscardAllocator(uint64_t fenceValue, ID3D12CommandAllocator* allocator);

	size_t Size() { return m_allocatorPool.size(); }

private:
	const D3D12_COMMAND_LIST_TYPE                            m_commandListType;
	ID3D12Device14*                                          m_device{ nullptr };
	std::vector<ID3D12CommandAllocator*>                     m_allocatorPool;
	std::queue<std::pair<uint64_t, ID3D12CommandAllocator*>> m_readyAllocators;
	std::mutex                                               m_allocatorMutex;
};

#endif // RENDER_D3D12