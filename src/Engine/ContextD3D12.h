#pragma once

#if RENDER_D3D12

struct ContextCreateInfo;

struct DeviceCapabilities final
{
	bool supportsHardwareRayTracing{ false };
	bool supportsWaveOps{ false };
	bool supportsFP16{ false };
	bool supportsMeshShaders{ false };
	bool supportsSamplerFeedback{ false };
	bool supportsTypedUAVLoads{ false };
	bool supportsEnhancedBarriers{ false };
	unsigned supportedMaxMultiSampleQualityLevel{ 0 };

	std::unordered_map<DXGI_FORMAT, bool> typedUAVLoadFormatSupportMap;
};

class ContextD3D12 final
{
public:
	~ContextD3D12();

	bool Create(const ContextCreateInfo& createInfo);
	void Destroy();

	auto GetD3DFactory() { return m_factory; }
	auto GetD3DAdapter() { return m_adapter; }
	auto GetD3DDevice() const noexcept { return m_device; }
	auto GetD3DDeviceRef() const noexcept { return m_device.Get(); }
	auto GetD3DAllocator() const noexcept { return m_allocator; }

	bool IsSupportAllowTearing() const { return m_supportAllowTearing; }

	const DeviceCapabilities& GetDeviceCapabilities() const { return m_deviceCapabilities; }

private:
	void enableDebugLayer(const ContextCreateInfo& createInfo);
	bool createFactory();
	bool selectAdapter(bool useWarp);
	bool createDevice();
	void checkDeviceFeatureSupport();
	bool createAllocator(bool enableCPUAllocationCallbacks);

	ComPtr<IDXGIFactory7>         m_factory;
	ComPtr<IDXGIAdapter4>         m_adapter;
	bool                          m_supportAllowTearing{ false };
	ComPtr<ID3D12Device14>        m_device;
	ComPtr<D3D12MA::Allocator>    m_allocator;
	D3D12MA::ALLOCATION_CALLBACKS m_allocationCallbacks = {};
	DeviceCapabilities            m_deviceCapabilities;
};

#endif // RENDER_D3D12