#include "stdafx.h"
#if RENDER_D3D12
#include "CommandListManagerD3D12.h"
#include "RHICoreD3D12.h"
#include "Log.h"
//=============================================================================
CommandListManagerD3D12 gCommandManager;
//=============================================================================
CommandListManagerD3D12::CommandListManagerD3D12()
	: m_graphicsQueue(D3D12_COMMAND_LIST_TYPE_DIRECT)
	, m_computeQueue(D3D12_COMMAND_LIST_TYPE_COMPUTE)
	, m_copyQueue(D3D12_COMMAND_LIST_TYPE_COPY)
{
}
//=============================================================================
CommandListManagerD3D12::~CommandListManagerD3D12()
{
	Shutdown();
}
//=============================================================================
void CommandListManagerD3D12::Create(ID3D12Device14* device)
{
	assert(device != nullptr);

	m_device = device;

	m_graphicsQueue.Create(device);
	m_computeQueue.Create(device);
	m_copyQueue.Create(device);
}
//=============================================================================
void CommandListManagerD3D12::Shutdown()
{
	m_graphicsQueue.Shutdown();
	m_computeQueue.Shutdown();
	m_copyQueue.Shutdown();
}
//=============================================================================
void CommandListManagerD3D12::CreateNewCommandList(D3D12_COMMAND_LIST_TYPE type, ID3D12GraphicsCommandList** list, ID3D12CommandAllocator** allocator)
{
	assert(type != D3D12_COMMAND_LIST_TYPE_BUNDLE, "Bundles are not yet supported");
	switch (type)
	{
	case D3D12_COMMAND_LIST_TYPE_DIRECT: *allocator = m_graphicsQueue.requestAllocator(); break;
	case D3D12_COMMAND_LIST_TYPE_BUNDLE: break;
	case D3D12_COMMAND_LIST_TYPE_COMPUTE: *allocator = m_computeQueue.requestAllocator(); break;
	case D3D12_COMMAND_LIST_TYPE_COPY: *allocator = m_copyQueue.requestAllocator(); break;
	}

	HRESULT result = m_device->CreateCommandList(1, type, *allocator, nullptr, IID_PPV_ARGS(list));
	if (FAILED(result))
	{
		Fatal("ID3D12Device14::CreateCommandList() failed: " + DXErrorToStr(result));
		return;
	}
	(*list)->SetName(L"CommandList");
}
//=============================================================================
void CommandListManagerD3D12::WaitForFence(uint64_t fenceValue)
{
	CommandQueueD3D12& producer = gCommandManager.GetQueue((D3D12_COMMAND_LIST_TYPE)(fenceValue >> 56));
	producer.WaitForFence(fenceValue);
}
//=============================================================================
#endif // RENDER_D3D12