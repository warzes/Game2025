#pragma once

#if RENDER_D3D12

#include "RHICoreD3D12.h"

class DescriptorHeapD3D12
{
public:
	DescriptorHeapD3D12(ComPtr<ID3D12Device14> device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, uint32_t numDescriptors, bool isShaderVisible);

	auto GetD3DHeap() const { return m_descriptorHeap; }
	auto GetHeapCPUStart() const { return m_CPU; }
	auto GetHeapGPUStart() const { return m_GPU; }
	auto GetMaxDescriptors() const { return m_maxDescriptors; }
	auto GetDescriptorSize() const { return m_descriptorSize; }

protected:
	D3D12_DESCRIPTOR_HEAP_TYPE   m_heapType{ D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES };
	uint32_t                     m_maxDescriptors{ 0 };
	ComPtr<ID3D12DescriptorHeap> m_descriptorHeap;
	D3D12_CPU_DESCRIPTOR_HANDLE  m_CPU;
	D3D12_GPU_DESCRIPTOR_HANDLE  m_GPU;
	uint32_t                     m_descriptorSize{ 0 };
	bool                         m_isShaderVisible{ false };
};

class StagingDescriptorHeapD3D12 final : public DescriptorHeapD3D12
{
public:
	StagingDescriptorHeapD3D12(ComPtr<ID3D12Device14> device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, uint32_t numDescriptors);
	~StagingDescriptorHeapD3D12();

	DescriptorD3D12 GetNewDescriptor();
	void FreeDescriptor(DescriptorD3D12& descriptor);

private:
	std::vector<uint32_t> m_freeDescriptors;
	uint32_t              m_currentDescriptorIndex{ 0 };
	uint32_t              m_activeHandleCount{ 0 };
	std::mutex            m_usageMutex;
};

class RenderPassDescriptorHeapD3D12 final : public DescriptorHeapD3D12
{
public:
	RenderPassDescriptorHeapD3D12(ComPtr<ID3D12Device14> device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, uint32_t reservedCount, uint32_t userCount);

	void Reset();
	DescriptorD3D12 AllocateUserDescriptorBlock(uint32_t count);
	DescriptorD3D12 GetReservedDescriptor(uint32_t index);

private:
	uint32_t   m_reservedHandleCount{ 0 };
	uint32_t   m_currentDescriptorIndex{ 0 };
	std::mutex m_usageMutex;
};

#endif // RENDER_D3D12