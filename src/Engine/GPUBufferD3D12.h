#pragma once

#if RENDER_D3D12

#include "RHICoreD3D12.h"

class GPUBufferD3D12
{
	// TODO:
};

struct VertexBufferCreateInfo final
{
	size_t vertexSize{ 0 };
	size_t vertexCount{ 0 };
};

class VertexBufferD3D12 final
{
public:
	bool Create(const VertexBufferCreateInfo& createInfo);
	void Destroy();

	[[nodiscard]] UINT8* Map();
	void Unmap();

	operator ComPtr<ID3D12Resource>() { return m_buffer; }
	operator ID3D12Resource* () { return m_buffer.Get(); }
	const D3D12_VERTEX_BUFFER_VIEW& View() const { return m_bufferView; }

private:
	ComPtr<ID3D12Resource>   m_buffer;
	D3D12_VERTEX_BUFFER_VIEW m_bufferView;
	size_t                   m_vertexSize{ 0 };
	size_t                   m_vertexCount{ 0 };
};

struct IndexBufferCreateInfo final
{
	size_t indexSize{ 0 };
	size_t indexCount{ 0 };
};

class IndexBufferD3D12 final
{
public:
	bool Create(const IndexBufferCreateInfo& createInfo);
	void Destroy();

	[[nodiscard]] UINT8* Map();
	void Unmap();

	operator ComPtr<ID3D12Resource>() { return m_buffer; }
	operator ID3D12Resource* () { return m_buffer.Get(); }
	const D3D12_INDEX_BUFFER_VIEW& View() const { return m_bufferView; }

private:
	ComPtr<ID3D12Resource>  m_buffer;
	D3D12_INDEX_BUFFER_VIEW m_bufferView;
	size_t                  m_indexSize{ 0 };
	size_t                  m_indexCount{ 0 };
};

struct ConstantBufferCreateInfo final
{
	size_t                size{ 0 };
	DescriptorHandleD3D12 descriptor{};
};

class ConstantBufferD3D12 final
{
public:
	bool Create(const ConstantBufferCreateInfo& createInfo);
	void Destroy();

	template<typename T>
	void CopyData(const T& data)
	{
		memcpy(m_mappedData, &data, sizeof(T));
	}

	operator ComPtr<ID3D12Resource>() { return m_buffer; }
	operator ID3D12Resource*() { return m_buffer.Get(); }
	const D3D12_CONSTANT_BUFFER_VIEW_DESC& View() const { return m_bufferView; }

private:
	ComPtr<ID3D12Resource>          m_buffer;
	D3D12_CONSTANT_BUFFER_VIEW_DESC m_bufferView;
	size_t                          m_size{ 0 };
	UINT8*                          m_mappedData{ nullptr };
};

#endif // RENDER_D3D12