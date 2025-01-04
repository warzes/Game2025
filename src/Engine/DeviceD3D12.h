#pragma once

#if RENDER_D3D12

class DeviceD3D12 final
{
public:
	~DeviceD3D12();

	bool Create(ComPtr<IDXGIAdapter4> adapter);
	void Destroy();

	auto GetD3DDevice() const noexcept { return m_device.Get(); }

private:
	bool createDevice(ComPtr<IDXGIAdapter4> adapter);
	bool createAllocator(ComPtr<IDXGIAdapter4> adapter);

	ComPtr<ID3D12Device14>     m_device;
	ComPtr<D3D12MA::Allocator> m_allocator;
	CD3DX12FeatureSupport      m_d3d12Features{};
};

#endif // RENDER_D3D12