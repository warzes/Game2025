#pragma once

#if RENDER_D3D12

#include "RenderCoreD3D12.h"

class DescriptorHeap
{
public:
	DescriptorHeap(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, uint32_t numDescriptors, bool isShaderVisible);
	virtual ~DescriptorHeap();

	auto GetHeap() const { return m_descriptorHeap; }
	auto GetHeapType() const { return m_heapType; }
	auto GetHeapCPUStart() const { return m_heapStart.CPUHandle; }
	auto GetHeapGPUStart() const { return m_heapStart.GPUHandle; }
	auto GetMaxDescriptors() const { return m_maxDescriptors; }
	auto GetDescriptorSize() const { return m_descriptorSize; }

protected:
	D3D12_DESCRIPTOR_HEAP_TYPE m_heapType{ D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES };
	uint32_t                   m_maxDescriptors{ 0 };
	uint32_t                   m_descriptorSize{ 0 };
	bool                       m_isShaderVisible{ false };
	ID3D12DescriptorHeap*      m_descriptorHeap{ nullptr };
	Descriptor                 m_heapStart{};
};

class StagingDescriptorHeap final : public DescriptorHeap
{
public:
	StagingDescriptorHeap(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, uint32_t numDescriptors);
	~StagingDescriptorHeap();

	Descriptor GetNewDescriptor();
	void FreeDescriptor(Descriptor descriptor);

private:
	std::vector<uint32_t> m_freeDescriptors;
	uint32_t              m_currentDescriptorIndex{ 0 };
	uint32_t              m_activeHandleCount{ 0 };
	std::mutex            m_usageMutex;
};

class RenderPassDescriptorHeap final : public DescriptorHeap
{
public:
	RenderPassDescriptorHeap(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, uint32_t reservedCount, uint32_t userCount);

	void Reset();
	Descriptor AllocateUserDescriptorBlock(uint32_t count);
	Descriptor GetReservedDescriptor(uint32_t index);

private:
	uint32_t   m_reservedHandleCount{ 0 };
	uint32_t   m_currentDescriptorIndex{ 0 };
	std::mutex m_usageMutex;
};

#endif // RENDER_D3D12