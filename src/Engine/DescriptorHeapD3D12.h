#pragma once

#if RENDER_D3D12

#include "RHICoreD3D12.h"

class DescriptorHeapD3D12
{
public:
	DescriptorHeapD3D12(ComPtr<ID3D12Device14> device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, uint32_t numDescriptors, bool isShaderVisible);

	auto GetD3DHeap() const { return m_descriptorHeap; }
	auto GetHeapCPUStart() const { return m_CPUStart; }
	auto GetHeapGPUStart() const { return m_GPUStart; }
	auto GetMaxDescriptors() const { return m_maxDescriptors; }
	auto GetDescriptorSize() const { return m_descriptorSize; }

	void AddToHandle(ComPtr<ID3D12Device14> device, DescriptorHandleD3D12& destCPUHandle, DescriptorHandleD3D12& sourceCPUHandle)
	{
		device->CopyDescriptorsSimple(1, destCPUHandle.CPUHandle, sourceCPUHandle.CPUHandle, m_heapType);
		destCPUHandle.CPUHandle.ptr += m_descriptorSize;
	}

	void AddToHandle(ComPtr<ID3D12Device14> device, DescriptorHandleD3D12& destCPUHandle, D3D12_CPU_DESCRIPTOR_HANDLE& sourceCPUHandle)
	{
		device->CopyDescriptorsSimple(1, destCPUHandle.CPUHandle, sourceCPUHandle, m_heapType);
		destCPUHandle.CPUHandle.ptr += m_descriptorSize;
	}

protected:
	ComPtr<ID3D12DescriptorHeap> m_descriptorHeap{};
	D3D12_DESCRIPTOR_HEAP_TYPE   m_heapType{};
	D3D12_CPU_DESCRIPTOR_HANDLE  m_CPUStart{ 0 };
	D3D12_GPU_DESCRIPTOR_HANDLE  m_GPUStart{ 0 };
	uint32_t                     m_maxDescriptors{ 0 };
	uint32_t                     m_descriptorSize{ 0 };
	bool                         m_isShaderVisible{ false };
};

// TODO: а может переименовать в СPUDescriptorHeapD3D12?
class StagingDescriptorHeapD3D12 final : public DescriptorHeapD3D12
{
public:
	StagingDescriptorHeapD3D12(ComPtr<ID3D12Device14> device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, uint32_t numDescriptors);
	~StagingDescriptorHeapD3D12();

	DescriptorHandleD3D12 GetNewDescriptor();
	void FreeDescriptor(DescriptorHandleD3D12& descriptor);

private:
	std::vector<uint32_t> m_freeDescriptors;
	uint32_t              m_currentDescriptorIndex{ 0 };
	uint32_t              m_activeHandleCount{ 0 };
	std::mutex            m_usageMutex;
};

// TODO: переименовать. и возможно совместить с RenderPassDescriptorHeapD3D12
class GPUDescriptorHeapD3D12 final : public DescriptorHeapD3D12
{
public:
	GPUDescriptorHeapD3D12(ComPtr<ID3D12Device14> device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, uint32_t numDescriptors);
	~GPUDescriptorHeapD3D12();

	void Reset();
	DescriptorHandleD3D12 GetHandleBlock(uint32_t count);

private:
	uint32_t   m_currentDescriptorIndex{ 0 };
	std::mutex m_usageMutex;
};

// TODO: или совместить с GPUDescriptorHeapD3D12, или удалить
class RenderPassDescriptorHeapD3D12 final : public DescriptorHeapD3D12
{
public:
	RenderPassDescriptorHeapD3D12(ComPtr<ID3D12Device14> device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, uint32_t reservedCount, uint32_t userCount);

	void Reset();
	DescriptorHandleD3D12 AllocateUserDescriptorBlock(uint32_t count);
	DescriptorHandleD3D12 GetReservedDescriptor(uint32_t index);

private:
	uint32_t   m_reservedHandleCount{ 0 };
	uint32_t   m_currentDescriptorIndex{ 0 };
	std::mutex m_usageMutex;
};

#endif // RENDER_D3D12