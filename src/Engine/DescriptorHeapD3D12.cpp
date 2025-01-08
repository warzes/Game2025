#include "stdafx.h"
#if RENDER_D3D12
#include "DescriptorHeapD3D12.h"
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

	m_heapStart.CPUHandle = m_descriptorHeap->GetCPUDescriptorHandleForHeapStart();

	if (m_isShaderVisible)
		m_heapStart.GPUHandle = m_descriptorHeap->GetGPUDescriptorHandleForHeapStart();

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
DescriptorD3D12 StagingDescriptorHeapD3D12::GetNewDescriptor()
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

	D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = m_heapStart.CPUHandle;
	cpuHandle.ptr += static_cast<uint64_t>(newHandleID) * m_descriptorSize;

	DescriptorD3D12 newDescriptor;
	newDescriptor.CPUHandle = cpuHandle;
	newDescriptor.heapIndex = newHandleID;

	m_activeHandleCount++;

	return newDescriptor;
}
//=============================================================================
void StagingDescriptorHeapD3D12::FreeDescriptor(DescriptorD3D12& descriptor)
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
#endif // RENDER_D3D12