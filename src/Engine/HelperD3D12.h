#pragma once

#if RENDER_D3D12

#include "Log.h"
#include "RHICoreD3D12.h"

// Creates a shader resource view from an arbitrary resource
bool CreateShaderResourceView(ComPtr<ID3D12Device14> device, ComPtr<ID3D12Resource> tex, D3D12_CPU_DESCRIPTOR_HANDLE srvDescriptor, bool isCubeMap = false);

// Creates an unordered access view from an arbitrary resource
bool CreateUnorderedAccessView(ComPtr<ID3D12Device14> device, ComPtr<ID3D12Resource> tex, D3D12_CPU_DESCRIPTOR_HANDLE uavDescriptor, uint32_t mipLevel = 0);

// Creates an render target view from an arbitrary resource
bool CreateRenderTargetView(ComPtr<ID3D12Device14> device, ComPtr<ID3D12Resource> tex, D3D12_CPU_DESCRIPTOR_HANDLE rtvDescriptor, uint32_t mipLevel = 0);

// Creates a shader resource view from a buffer resource
bool CreateBufferShaderResourceView(ComPtr<ID3D12Device14> device, ComPtr<ID3D12Resource> buffer, D3D12_CPU_DESCRIPTOR_HANDLE srvDescriptor, uint32_t stride = 0);

// Creates a unordered access view from a buffer resource
bool CreateBufferUnorderedAccessView(ComPtr<ID3D12Device14> device, ComPtr<ID3D12Resource> buffer, D3D12_CPU_DESCRIPTOR_HANDLE uavDescriptor, uint32_t stride, D3D12_BUFFER_UAV_FLAGS flag = D3D12_BUFFER_UAV_FLAG_NONE, uint32_t counterOffset = 0, ComPtr<ID3D12Resource> counterResource = nullptr);

// Shorthand for creating a root signature
inline ComPtr<ID3D12RootSignature> CreateRootSignature(ComPtr<ID3D12Device14> device, const D3D12_ROOT_SIGNATURE_DESC& rootSignatureDesc) noexcept
{
	ComPtr<ID3DBlob> signature;
	ComPtr<ID3DBlob> error;
	HRESULT result = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, signature.GetAddressOf(), error.GetAddressOf());
	if (!SUCCEEDED(result))
	{
		const char* err = (char*)error->GetBufferPointer();
		Fatal("D3D12SerializeRootSignature failed: " + std::string(err));
		return nullptr;
	}
	ComPtr<ID3D12RootSignature> rootSignature;
	result = device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&rootSignature));
	if (!SUCCEEDED(result))
	{
		Fatal("ID3D12Device14::CreateRootSignature() failed: " + DXErrorToStr(result));
		return nullptr;
	}

	return rootSignature;
}

// Helper for resource barrier.
inline void TransitionResource(ComPtr<ID3D12GraphicsCommandList> commandList, ComPtr<ID3D12Resource> resource, D3D12_RESOURCE_STATES stateBefore, D3D12_RESOURCE_STATES stateAfter) noexcept
{
	assert(commandList != nullptr);
	assert(resource != nullptr);

	if (stateBefore == stateAfter)
		return;

	D3D12_RESOURCE_BARRIER desc = {};
	desc.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	desc.Transition.pResource   = resource.Get();
	desc.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	desc.Transition.StateBefore = stateBefore;
	desc.Transition.StateAfter  = stateAfter;

	commandList->ResourceBarrier(1, &desc);
}

// Helper which applies one or more resources barriers and then reverses them on destruction.
class ScopedBarrier final
{
public:
	ScopedBarrier(ComPtr<ID3D12GraphicsCommandList> commandList, std::initializer_list<D3D12_RESOURCE_BARRIER> barriers)
		: m_commandList(commandList)
		, m_barriers(barriers)
	{
		assert(m_barriers.size() <= UINT32_MAX);
		m_commandList->ResourceBarrier(static_cast<UINT>(m_barriers.size()), m_barriers.data());
	}

	ScopedBarrier(ComPtr<ID3D12GraphicsCommandList> commandList, const D3D12_RESOURCE_BARRIER* barriers, size_t count)
		: m_commandList(commandList)
		, m_barriers(barriers, barriers + count)
	{
		assert(count <= UINT32_MAX);
		m_commandList->ResourceBarrier(static_cast<UINT>(m_barriers.size()), m_barriers.data());
	}

	template<size_t TBarrierLength>
	ScopedBarrier(ComPtr<ID3D12GraphicsCommandList> commandList, const D3D12_RESOURCE_BARRIER(&barriers)[TBarrierLength])
		: m_commandList(commandList)
		, m_barriers(barriers, barriers + TBarrierLength)
	{
		assert(TBarrierLength <= UINT32_MAX);
		m_commandList->ResourceBarrier(static_cast<UINT>(m_barriers.size()), m_barriers.data());
	}

	ScopedBarrier(ScopedBarrier&&) = default;
	ScopedBarrier(const ScopedBarrier&) = delete;
	ScopedBarrier& operator=(ScopedBarrier&&) = default;
	ScopedBarrier& operator=(const ScopedBarrier&) = delete;

	~ScopedBarrier()
	{
		for (auto& b : m_barriers)
		{
			std::swap(b.Transition.StateAfter, b.Transition.StateBefore);
		}
		m_commandList->ResourceBarrier(static_cast<UINT>(m_barriers.size()), m_barriers.data());
	}

private:
	ComPtr<ID3D12GraphicsCommandList>   m_commandList;
	std::vector<D3D12_RESOURCE_BARRIER> m_barriers;
};

// Helper for obtaining texture size
inline glm::uvec2 GetTextureSize(_In_ ID3D12Resource* tex) noexcept
{
	const auto desc = tex->GetDesc();
	return { static_cast<uint32_t>(desc.Width), static_cast<uint32_t>(desc.Height) };
}

template<class... Args>
[[nodiscard]] inline void SetDebugObjectName(ID3D12Object* pObj, const char* format, Args&&... args)
{
	char bufName[240];
	sprintf_s(bufName, format, args...);
	std::string Name = bufName;
	std::wstring wName(Name.begin(), Name.end());
	HRESULT result = pObj->SetName(wName.c_str());
	if (FAILED(result))
		Fatal("SetName() failed: " + DXErrorToStr(result));
}

class ScopedPixEvent final
{
public:
	ScopedPixEvent(ComPtr<ID3D12GraphicsCommandList> commandList, PCWSTR pFormat) noexcept
		: m_commandList(commandList)
	{
		PIXBeginEvent(m_commandList.Get(), 0, pFormat);
	}
	~ScopedPixEvent()
	{
		PIXEndEvent(m_commandList.Get());
	}

private:
	ComPtr<ID3D12GraphicsCommandList> m_commandList;
};

#endif // RENDER_D3D12