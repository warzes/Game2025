#pragma once

#if RENDER_D3D12

class FenceD3D12;

class CommandQueueD3D12 final
{
public:
	bool Create(ID3D12Device14* device, D3D12_COMMAND_LIST_TYPE type, const char* name = nullptr);
	void Destroy();

	bool Signal(const FenceD3D12& fence, uint64_t FenceWaitValue);

	void WaitOnGPU(const FenceD3D12& fence, uint64_t FenceWaitValue);

	operator ComPtr<ID3D12CommandQueue>() const { return m_queue; }
	operator ID3D12CommandQueue*() const { return m_queue.Get(); }
	auto Get() const { return m_queue; }
	auto GetRef() const { return m_queue.Get(); }

private:
	ComPtr<ID3D12CommandQueue> m_queue;
};

#endif // RENDER_D3D12