#pragma once

#if RENDER_D3D12

#include "DescriptorHeapD3D12.h"

// TODO: функцию CreateCPUHandle и переменную m_CPUDescriptorHeaps/m_GPUDescriptorHeaps разбить на типы дескриптов.
// - нужен метод удаления
// - нужно ли держать RTV/DSV кучи под каждую MAX_BACK_BUFFER_COUNT? или таки один хватит?

// на текущий момент буду пользоваться в основном кучами по 0 бекбуферу. надо подумать - нужно ли создавать их копии в числе MAX_BACK_BUFFER_COUNT

class DescriptorHeapManagerD3D12 final
{
public:
	bool Create(ComPtr<ID3D12Device14> device);
	void Destroy();

	DescriptorHandleD3D12 CreateCPUHandle(D3D12_DESCRIPTOR_HEAP_TYPE heapType, size_t frameIndex);
	DescriptorHandleD3D12 CreateGPUHandle(D3D12_DESCRIPTOR_HEAP_TYPE heapType, size_t frameIndex, uint32_t count);

	GPUDescriptorHeapD3D12* GetGPUHeap(D3D12_DESCRIPTOR_HEAP_TYPE heapType, size_t backBufferIndex)
	{
		return m_GPUDescriptorHeaps[backBufferIndex][heapType];
	}

private:
	StagingDescriptorHeapD3D12* m_CPUDescriptorHeaps[MAX_BACK_BUFFER_COUNT][D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES]{ nullptr };
	GPUDescriptorHeapD3D12*     m_GPUDescriptorHeaps[MAX_BACK_BUFFER_COUNT][D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES]{ nullptr };
};

#endif // RENDER_D3D12