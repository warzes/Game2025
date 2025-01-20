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

	auto GetFactory() { return m_factory; }
	auto GetAdapter() { return m_adapter; }
	auto GetDevice() const noexcept { return m_device; }
	auto GetD3DDeviceRef() const noexcept { return m_device.Get(); }
	auto GetAllocator() const noexcept { return m_allocator; }

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
	ComPtr<ID3D12Device14>        m_device;
	ComPtr<D3D12MA::Allocator>    m_allocator;
	D3D12MA::ALLOCATION_CALLBACKS m_allocationCallbacks = {};
	DeviceCapabilities            m_deviceCapabilities;
};

#endif // RENDER_D3D12