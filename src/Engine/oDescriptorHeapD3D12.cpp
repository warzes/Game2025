#include "stdafx.h"
#if RENDER_D3D12
#include "oDescriptorHeapD3D12.h"
#include "Log.h"
//=============================================================================
DescriptorHeap::DescriptorHeap(ComPtr<ID3D12Device> device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, uint32_t numDescriptors, bool isShaderVisible)
	: m_heapType(heapType)
	, m_maxDescriptors(numDescriptors)
	, m_isShaderVisible(isShaderVisible)
{
	D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
	heapDesc.NumDescriptors = m_maxDescriptors;
	heapDesc.Type           = m_heapType;
	heapDesc.Flags          = m_isShaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	heapDesc.NodeMask       = 0;

	HRESULT result = device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_descriptorHeap));
	if (FAILED(result))
	{
		Fatal("ID3D12Device:CreateDescriptorHeap:() failed: " + DXErrorToStr(result));
		return;
	}

	m_heapStart.CPUHandle = m_descriptorHeap->GetCPUDescriptorHandleForHeapStart();

	if (m_isShaderVisible)
		m_heapStart.GPUHandle = m_descriptorHeap->GetGPUDescriptorHandleForHeapStart();

	m_descriptorSize = device->GetDescriptorHandleIncrementSize(m_heapType);
}
//=============================================================================
StagingDescriptorHeap::StagingDescriptorHeap(ComPtr<ID3D12Device> device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, uint32_t numDescriptors)
	: DescriptorHeap(device, heapType, numDescriptors, false)
{
	m_freeDescriptors.reserve(numDescriptors);
}
//=============================================================================
StagingDescriptorHeap::~StagingDescriptorHeap()
{
	if (m_activeHandleCount != 0)
	{
		Fatal("There were active handles when the descriptor heap was destroyed. Look for leaks.");
	}
}
//=============================================================================
Descriptor StagingDescriptorHeap::GetNewDescriptor()
{
	std::lock_guard<std::mutex> lockGuard(m_usageMutex);

	uint32_t newHandleID = 0;

	if (m_currentDescriptorIndex < m_maxDescriptors)
	{
		newHandleID = m_currentDescriptorIndex;
		m_currentDescriptorIndex++;
	}
	else if (m_freeDescriptors.size() > 0)
	{
		newHandleID = m_freeDescriptors.back();
		m_freeDescriptors.pop_back();
	}
	else
	{
		Fatal("Ran out of dynamic descriptor heap handles, need to increase heap size.");
	}

	D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = m_heapStart.CPUHandle;
	cpuHandle.ptr += static_cast<uint64_t>(newHandleID) * m_descriptorSize;

	Descriptor newDescriptor;
	newDescriptor.CPUHandle = cpuHandle;
	newDescriptor.heapIndex = newHandleID;

	m_activeHandleCount++;

	return newDescriptor;
}
//=============================================================================
void StagingDescriptorHeap::FreeDescriptor(Descriptor descriptor)
{
	std::lock_guard<std::mutex> lockGuard(m_usageMutex);

	m_freeDescriptors.push_back(descriptor.heapIndex);

	if (m_activeHandleCount == 0)
	{
		Fatal("Freeing heap handles when there should be none left");
		return;
	}

	m_activeHandleCount--;
}
//=============================================================================
RenderPassDescriptorHeap::RenderPassDescriptorHeap(ComPtr<ID3D12Device> device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, uint32_t reservedCount, uint32_t userCount)
	: DescriptorHeap(device, heapType, reservedCount + userCount, true)
	, m_reservedHandleCount(reservedCount)
	, m_currentDescriptorIndex(reservedCount)
{
}
//=============================================================================
Descriptor RenderPassDescriptorHeap::GetReservedDescriptor(uint32_t index)
{
	assert(index < m_reservedHandleCount);

	D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = m_heapStart.CPUHandle;
	D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = m_heapStart.GPUHandle;
	cpuHandle.ptr += static_cast<uint64_t>(index) * m_descriptorSize;
	gpuHandle.ptr += static_cast<uint64_t>(index) * m_descriptorSize;

	Descriptor descriptor;
	descriptor.heapIndex = index;
	descriptor.CPUHandle = cpuHandle;
	descriptor.GPUHandle = gpuHandle;

	return descriptor;
}
//=============================================================================
Descriptor RenderPassDescriptorHeap::AllocateUserDescriptorBlock(uint32_t count)
{
	uint32_t newHandleID = 0;

	{
		std::lock_guard<std::mutex> lockGuard(m_usageMutex);

		uint32_t blockEnd = m_currentDescriptorIndex + count;

		if (blockEnd <= m_maxDescriptors)
		{
			newHandleID = m_currentDescriptorIndex;
			m_currentDescriptorIndex = blockEnd;
		}
		else
		{
			Fatal("Ran out of render pass descriptor heap handles, need to increase heap size.");
		}
	}

	Descriptor newDescriptor;
	newDescriptor.heapIndex = newHandleID;

	D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = m_heapStart.CPUHandle;
	cpuHandle.ptr += static_cast<uint64_t>(newHandleID) * m_descriptorSize;
	newDescriptor.CPUHandle = cpuHandle;

	D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = m_heapStart.GPUHandle;
	gpuHandle.ptr += static_cast<uint64_t>(newHandleID) * m_descriptorSize;
	newDescriptor.GPUHandle = gpuHandle;

	return newDescriptor;
}
//=============================================================================
void RenderPassDescriptorHeap::Reset()
{
	m_currentDescriptorIndex = m_reservedHandleCount;
}
//=============================================================================

#endif // RENDER_D3D12