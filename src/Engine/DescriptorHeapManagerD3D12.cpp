#include "stdafx.h"
#if RENDER_D3D12
#include "DescriptorHeapManagerD3D12.h"
//=============================================================================
bool DescriptorHeapManagerD3D12::Create(ComPtr<ID3D12Device14> device)
{
	for (int i = 0; i < MAX_BACK_BUFFER_COUNT; i++)
	{
		ZeroMemory(m_CPUDescriptorHeaps[i], sizeof(m_CPUDescriptorHeaps[i]));
		m_CPUDescriptorHeaps[i][D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV] = new StagingDescriptorHeapD3D12(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, NUM_SRV_STAGING_DESCRIPTORS);
		m_CPUDescriptorHeaps[i][D3D12_DESCRIPTOR_HEAP_TYPE_RTV]         = new StagingDescriptorHeapD3D12(device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, NUM_RTV_STAGING_DESCRIPTORS);
		m_CPUDescriptorHeaps[i][D3D12_DESCRIPTOR_HEAP_TYPE_DSV]         = new StagingDescriptorHeapD3D12(device, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, NUM_DSV_STAGING_DESCRIPTORS);
		m_CPUDescriptorHeaps[i][D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER]     = new StagingDescriptorHeapD3D12(device, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, NUM_SAMPLER_DESCRIPTORS);

		ZeroMemory(m_GPUDescriptorHeaps[i], sizeof(m_GPUDescriptorHeaps[i]));
		m_GPUDescriptorHeaps[i][D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV] = new GPUDescriptorHeapD3D12(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, NUM_SRV_STAGING_DESCRIPTORS);
		m_GPUDescriptorHeaps[i][D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER]     = new GPUDescriptorHeapD3D12(device, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, NUM_SAMPLER_DESCRIPTORS);
	}

	return true;
}
//=============================================================================
void DescriptorHeapManagerD3D12::Destroy()
{
	for (int j = 0; j < MAX_BACK_BUFFER_COUNT; j++)
	{
		for (int i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; i++)
		{
			delete m_CPUDescriptorHeaps[j][i]; m_CPUDescriptorHeaps[j][i] = nullptr;
			delete m_GPUDescriptorHeaps[j][i]; m_GPUDescriptorHeaps[j][i] = nullptr;
		}
	}
}
//=============================================================================
DescriptorHandleD3D12 DescriptorHeapManagerD3D12::CreateCPUHandle(D3D12_DESCRIPTOR_HEAP_TYPE heapType, size_t frameIndex)
{
	return m_CPUDescriptorHeaps[frameIndex][heapType]->GetNewDescriptor();
}
//=============================================================================
DescriptorHandleD3D12 DescriptorHeapManagerD3D12::CreateGPUHandle(D3D12_DESCRIPTOR_HEAP_TYPE heapType, size_t frameIndex, uint32_t count)
{
	return m_GPUDescriptorHeaps[frameIndex][heapType]->GetHandleBlock(count);
}
//=============================================================================
#endif // RENDER_D3D12