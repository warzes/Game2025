#include "stdafx.h"
#if RENDER_D3D12
#include "DescriptorHeapD3D12.h"
#include "HelperD3D12.h"
#include "Log.h"
//=============================================================================
DescriptorHeapD3D12::DescriptorHeapD3D12(ComPtr<ID3D12Device14> device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, uint32_t numDescriptors, bool isShaderVisible)
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
		Fatal("ID3D12Device::CreateDescriptorHeap() failed: " + DXErrorToStr(result));
		return;
	}
	SetDebugObjectName(m_descriptorHeap.Get(), "DescriptorHeap");

	m_CPUStart = m_descriptorHeap->GetCPUDescriptorHandleForHeapStart();

	if (m_isShaderVisible)
		m_GPUStart = m_descriptorHeap->GetGPUDescriptorHandleForHeapStart();

	m_descriptorSize = device->GetDescriptorHandleIncrementSize(m_heapType);
}
//=============================================================================
StagingDescriptorHeapD3D12::StagingDescriptorHeapD3D12(ComPtr<ID3D12Device14> device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, uint32_t numDescriptors)
	: DescriptorHeapD3D12(device, heapType, numDescriptors, false)
{
	m_freeDescriptors.reserve(numDescriptors);
}
//=============================================================================
StagingDescriptorHeapD3D12::~StagingDescriptorHeapD3D12()
{
	if (m_activeHandleCount != 0)
	{
		Fatal("There were active handles when the descriptor heap was destroyed. Look for leaks.");
	}
}
//=============================================================================
DescriptorHandleD3D12 StagingDescriptorHeapD3D12::GetNewDescriptor()
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
		return {};
	}

	D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = m_CPUStart;
	cpuHandle.ptr += static_cast<uint64_t>(newHandleID) * m_descriptorSize;

	DescriptorHandleD3D12 newDescriptor;
	newDescriptor.CPUHandle = cpuHandle;
	newDescriptor.heapIndex = newHandleID;

	m_activeHandleCount++;

	return newDescriptor;
}
//=============================================================================
void StagingDescriptorHeapD3D12::FreeDescriptor(DescriptorHandleD3D12& descriptor)
{
	std::lock_guard<std::mutex> lockGuard(m_usageMutex);

	m_freeDescriptors.push_back(descriptor.heapIndex);

	descriptor = {};

	if (m_activeHandleCount == 0)
	{
		Fatal("Freeing heap handles when there should be none left");
		return;
	}
	m_activeHandleCount--;
}
//=============================================================================
GPUDescriptorHeapD3D12::GPUDescriptorHeapD3D12(ComPtr<ID3D12Device14> device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, uint32_t numDescriptors)
	: DescriptorHeapD3D12(device, heapType, numDescriptors, true)
{
}
//=============================================================================
GPUDescriptorHeapD3D12::~GPUDescriptorHeapD3D12()
{
}
//=============================================================================
void GPUDescriptorHeapD3D12::Reset()
{
	m_currentDescriptorIndex = 0;
}
//=============================================================================
DescriptorHandleD3D12 GPUDescriptorHeapD3D12::GetHandleBlock(uint32_t count)
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

	D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = m_CPUStart;
	cpuHandle.ptr += static_cast<uint64_t>(newHandleID) * m_descriptorSize;

	D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = m_GPUStart;
	gpuHandle.ptr += static_cast<uint64_t>(newHandleID) * m_descriptorSize;

	DescriptorHandleD3D12 newDescriptor;
	newDescriptor.CPUHandle = cpuHandle;
	newDescriptor.GPUHandle = gpuHandle;
	newDescriptor.heapIndex = newHandleID;
	return newDescriptor;
}
//=============================================================================
RenderPassDescriptorHeapD3D12::RenderPassDescriptorHeapD3D12(ComPtr<ID3D12Device14> device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, uint32_t reservedCount, uint32_t userCount)
	: DescriptorHeapD3D12(device, heapType, reservedCount + userCount, true)
	, m_reservedHandleCount(reservedCount)
	, m_currentDescriptorIndex(reservedCount)
{
}
//=============================================================================
DescriptorHandleD3D12 RenderPassDescriptorHeapD3D12::GetReservedDescriptor(uint32_t index)
{
	assert(index < m_reservedHandleCount);

	D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = m_CPUStart;
	D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = m_GPUStart;
	cpuHandle.ptr += static_cast<uint64_t>(index) * m_descriptorSize;
	gpuHandle.ptr += static_cast<uint64_t>(index) * m_descriptorSize;

	DescriptorHandleD3D12 descriptor;
	descriptor.heapIndex = index;
	descriptor.CPUHandle = cpuHandle;
	descriptor.GPUHandle = gpuHandle;

	return descriptor;
}
//=============================================================================
DescriptorHandleD3D12 RenderPassDescriptorHeapD3D12::AllocateUserDescriptorBlock(uint32_t count)
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

	DescriptorHandleD3D12 newDescriptor;
	newDescriptor.heapIndex = newHandleID;

	D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = m_CPUStart;
	cpuHandle.ptr += static_cast<uint64_t>(newHandleID) * m_descriptorSize;
	newDescriptor.CPUHandle = cpuHandle;

	D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = m_GPUStart;
	gpuHandle.ptr += static_cast<uint64_t>(newHandleID) * m_descriptorSize;
	newDescriptor.GPUHandle = gpuHandle;

	return newDescriptor;
}
//=============================================================================
void RenderPassDescriptorHeapD3D12::Reset()
{
	m_currentDescriptorIndex = m_reservedHandleCount;
}
//=============================================================================
#endif // RENDER_D3D12