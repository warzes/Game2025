#include "stdafx.h"
#if RENDER_D3D12
#include "GPUBufferD3D12.h"
#include "RHIBackendD3D12.h"
#include "HelperD3D12.h"
//=============================================================================
bool VertexBufferD3D12::Create(const VertexBufferCreateInfo& createInfo)
{
	m_vertexSize = createInfo.vertexSize;
	m_vertexCount = createInfo.vertexCount;
	const size_t bufferSize = m_vertexSize * m_vertexCount;

	// Note: using upload heaps to transfer static data like vert buffers is not recommended. Every time the GPU needs it, the upload heap will be marshalled over. Please read up on Default Heap usage. An upload heap is used here for code simplicity and because there are very few verts to actually transfer.
	const auto heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	const auto desc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);
	HRESULT result = gRHI.GetD3DDevice()->CreateCommittedResource(
		&heapProp, 
		D3D12_HEAP_FLAG_NONE, 
		&desc, 
		D3D12_RESOURCE_STATE_GENERIC_READ, 
		nullptr, 
		IID_PPV_ARGS(&m_buffer));
	if (FAILED(result))
	{
		Fatal("ID3D12Device14::CreateCommittedResource() failed: " + DXErrorToStr(result));
		return false;
	}

	m_bufferView.BufferLocation = m_buffer->GetGPUVirtualAddress();
	m_bufferView.StrideInBytes = m_vertexSize;
	m_bufferView.SizeInBytes = bufferSize;

	return true;
}
//=============================================================================
void VertexBufferD3D12::Destroy()
{
	m_buffer.Reset();
}
//=============================================================================
UINT8* VertexBufferD3D12::Map()
{
	UINT8* dataBegin;
	CD3DX12_RANGE readRange(0, 0); // We do not intend to read from this resource on the CPU.
	HRESULT result = m_buffer->Map(0, &readRange, reinterpret_cast<void**>(&dataBegin));
	if (FAILED(result))
	{
		Fatal("ID3D12Device14::Map() failed: " + DXErrorToStr(result));
		return nullptr;
	}
	return dataBegin;
}
//=============================================================================
void VertexBufferD3D12::Unmap()
{
	m_buffer->Unmap(0, nullptr);
}
//=============================================================================
bool IndexBufferD3D12::Create(const IndexBufferCreateInfo& createInfo)
{
	m_indexSize = createInfo.indexSize;
	m_indexCount = createInfo.indexCount;
	const size_t bufferSize = m_indexSize * m_indexCount;

	// Note: using upload heaps to transfer static data like vert buffers is not recommended. Every time the GPU needs it, the upload heap will be marshalled over. Please read up on Default Heap usage. An upload heap is used here for code simplicity and because there are very few verts to actually transfer.
	const auto heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	const auto desc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);
	HRESULT result = gRHI.GetD3DDevice()->CreateCommittedResource(
		&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&desc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&m_buffer));
	if (FAILED(result))
	{
		Fatal("ID3D12Device14::CreateCommittedResource() failed: " + DXErrorToStr(result));
		return false;
	}

	m_bufferView.BufferLocation = m_buffer->GetGPUVirtualAddress();
	m_bufferView.SizeInBytes = bufferSize;
	m_bufferView.Format = m_indexSize == 4 ? DXGI_FORMAT_R32_UINT : m_indexSize == 2 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_UNKNOWN;

	return true;
}
//=============================================================================
void IndexBufferD3D12::Destroy()
{
	m_buffer.Reset();
}
//=============================================================================
UINT8* IndexBufferD3D12::Map()
{
	UINT8* dataBegin;
	CD3DX12_RANGE readRange(0, 0); // We do not intend to read from this resource on the CPU.
	HRESULT result = m_buffer->Map(0, &readRange, reinterpret_cast<void**>(&dataBegin));
	if (FAILED(result))
	{
		Fatal("ID3D12Device14::Map() failed: " + DXErrorToStr(result));
		return nullptr;
	}
	return dataBegin;
}
//=============================================================================
void IndexBufferD3D12::Unmap()
{
	m_buffer->Unmap(0, nullptr);
}
//=============================================================================
bool ConstantBufferD3D12::Create(const ConstantBufferCreateInfo& createInfo)
{
	// Constant buffer elements need to be multiples of 256 bytes.
	// This is because the hardware can only view constant data at m*256 byte offsets and of n*256 byte lengths. 
	// typedef struct D3D12_CONSTANT_BUFFER_VIEW_DESC {
	// UINT64 OffsetInBytes; // multiple of 256
	// UINT   SizeInBytes;   // multiple of 256
	// } D3D12_CONSTANT_BUFFER_VIEW_DESC;
	m_size = CalcConstantBufferByteSize(sizeof(createInfo.size));

	// Note: using upload heaps to transfer static data like vert buffers is not recommended. Every time the GPU needs it, the upload heap will be marshalled over. Please read up on Default Heap usage. An upload heap is used here for code simplicity and because there are very few verts to actually transfer.
	const auto heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	const auto desc = CD3DX12_RESOURCE_DESC::Buffer(m_size);

	HRESULT result = gRHI.GetD3DDevice()->CreateCommittedResource(
		&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&desc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&m_buffer));
	if (FAILED(result))
	{
		Fatal("ID3D12Device14::CreateCommittedResource() failed: " + DXErrorToStr(result));
		return false;
	}

	result = m_buffer->Map(0, nullptr, reinterpret_cast<void**>(&m_mappedData));
	// We do not need to unmap until we are done with the resource. However, we must not write to the resource while it is in use by the GPU (so we must use synchronization techniques).
	if (FAILED(result))
	{
		Fatal("ID3D12Device14::Map() failed: " + DXErrorToStr(result));
		return false;
	}

	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
	cbvDesc.BufferLocation = m_buffer->GetGPUVirtualAddress();
	cbvDesc.SizeInBytes = m_size;

	gRHI.GetD3DDevice()->CreateConstantBufferView(
		&cbvDesc,
		createInfo.descriptor.CPUHandle);

	return true;
}
//=============================================================================
void ConstantBufferD3D12::Destroy()
{
	if (m_buffer) m_buffer->Unmap(0, nullptr);
	m_mappedData = nullptr;
	m_buffer.Reset();
}
//=============================================================================
#endif // RENDER_D3D12