#pragma once

#if RENDER_D3D12

class FenceD3D12 final
{
public:
	bool Create(ID3D12Device14* device, const char* debugName = nullptr);
	void Destroy();

	bool WaitOnCPU(uint64_t FenceWaitValue) const;

	operator ComPtr<ID3D12Fence>() const { return m_fence; }
	operator ID3D12Fence*() const { return m_fence.Get(); }
	auto Get() const { return m_fence; }
	auto GetRef() const { return m_fence.Get(); }
	auto GetEvent() const { return m_event.Get(); }

	bool IsValid() const { return m_fence && m_event.IsValid(); }

private:
	Microsoft::WRL::Wrappers::Event m_event;
	ComPtr<ID3D12Fence>             m_fence;
};

#endif // RENDER_D3D12