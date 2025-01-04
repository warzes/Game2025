#include "stdafx.h"
#if RENDER_D3D12
#include "CommandAllocatorPoolD3D12.h"
#include "RHICoreD3D12.h"
#include "Log.h"
//=============================================================================
CommandAllocatorPoolD3D12::CommandAllocatorPoolD3D12(D3D12_COMMAND_LIST_TYPE type)
	: m_commandListType(type)
{
}
//=============================================================================
CommandAllocatorPoolD3D12::~CommandAllocatorPoolD3D12()
{
	Shutdown();
}
//=============================================================================
void CommandAllocatorPoolD3D12::Create(ID3D12Device14* device)
{
	m_device = device;
}
//=============================================================================
void CommandAllocatorPoolD3D12::Shutdown()
{
	for (size_t i = 0; i < m_allocatorPool.size(); ++i)
		m_allocatorPool[i]->Release();
	m_allocatorPool.clear();
}
//=============================================================================
ID3D12CommandAllocator* CommandAllocatorPoolD3D12::RequestAllocator(uint64_t completedFenceValue)
{
	std::lock_guard<std::mutex> LockGuard(m_allocatorMutex);

	ID3D12CommandAllocator* allocator = nullptr;

	if (!m_readyAllocators.empty())
	{
		auto& AllocatorPair = m_readyAllocators.front();

		if (AllocatorPair.first <= completedFenceValue)
		{
			allocator = AllocatorPair.second;
			HRESULT result = allocator->Reset();
			if (FAILED(result))
			{
				Fatal("ID3D12CommandAllocator::Reset() failed: " + DXErrorToStr(result));
				return nullptr;
			}
			m_readyAllocators.pop();
		}
	}

	// If no allocator's were ready to be reused, create a new one
	if (allocator == nullptr)
	{
		HRESULT result = m_device->CreateCommandAllocator(m_commandListType, IID_PPV_ARGS(&allocator));
		if (FAILED(result))
		{
			Fatal("ID3D12Device14::CreateCommandAllocator() failed: " + DXErrorToStr(result));
			return nullptr;
		}
		wchar_t AllocatorName[32];
		swprintf(AllocatorName, 32, L"CommandAllocator %zu", m_allocatorPool.size());
		allocator->SetName(AllocatorName);
		m_allocatorPool.push_back(allocator);
	}

	return allocator;
}
//=============================================================================
void CommandAllocatorPoolD3D12::DiscardAllocator(uint64_t fenceValue, ID3D12CommandAllocator* allocator)
{
	std::lock_guard<std::mutex> LockGuard(m_allocatorMutex);
	// That fence value indicates we are free to reset the allocator
	m_readyAllocators.push(std::make_pair(fenceValue, allocator));
}
//=============================================================================
#endif // RENDER_D3D12