#pragma once

#if RENDER_D3D12

#include "DeviceD3D12.h"

struct ContextCreateInfo;

class ContextD3D12 final
{
public:
	~ContextD3D12();

	bool Create(const ContextCreateInfo& createInfo);
	void Destroy();

	auto GetFactory() { return m_factory; }
	auto GetAdapter() { return m_adapter; }
	bool IsSupportAllowTearing() const { return m_supportAllowTearing; }

	auto* GetDevice() const { return &m_device; }

private:
	void enableDebugLayer(const ContextCreateInfo& createInfo);
	bool createFactory();
	bool selectAdapter(bool useWarp);

	ComPtr<IDXGIFactory7> m_factory;
	ComPtr<IDXGIAdapter4> m_adapter;
	bool                  m_supportAllowTearing{ false };
	DeviceD3D12           m_device{};
};

#endif // RENDER_D3D12