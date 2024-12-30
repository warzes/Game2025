#include "stdafx.h"
#if RENDER_D3D12
#include "oRenderCoreD3D12.h"
#include "Log.h"
//=============================================================================
void PipelineResourceSpace::SetCBV(BufferResource* resource)
{
	if (m_isLocked)
	{
		if (m_CBV == nullptr)
		{
			Fatal("Setting unused binding in a locked resource space");
		}
		else
		{
			m_CBV = resource;
		}
	}
	else
	{
		m_CBV = resource;
	}
}
//=============================================================================
void PipelineResourceSpace::SetSRV(const PipelineResourceBinding& binding)
{
	uint32_t currentIndex = getIndexOfBindingIndex(m_SRVs, binding.bindingIndex);

	if (m_isLocked)
	{
		if (currentIndex == UINT_MAX)
		{
			Fatal("Setting unused binding in a locked resource space");
		}
		else
		{
			m_SRVs[currentIndex] = binding;
		}
	}
	else
	{
		if (currentIndex == UINT_MAX)
		{
			m_SRVs.push_back(binding);
			std::sort(m_SRVs.begin(), m_SRVs.end(), SortPipelineBindings);
		}
		else
		{
			m_SRVs[currentIndex] = binding;
		}
	}
}
//=============================================================================
void PipelineResourceSpace::SetUAV(const PipelineResourceBinding& binding)
{
	uint32_t currentIndex = getIndexOfBindingIndex(m_UAVs, binding.bindingIndex);

	if (m_isLocked)
	{
		if (currentIndex == UINT_MAX)
		{
			Fatal("Setting unused binding in a locked resource space");
		}
		else
		{
			m_UAVs[currentIndex] = binding;
		}
	}
	else
	{
		if (currentIndex == UINT_MAX)
		{
			m_UAVs.push_back(binding);
			std::sort(m_UAVs.begin(), m_UAVs.end(), SortPipelineBindings);
		}
		else
		{
			m_UAVs[currentIndex] = binding;
		}
	}
}
//=============================================================================
void PipelineResourceSpace::Lock()
{
	m_isLocked = true;
}
//=============================================================================
uint32_t PipelineResourceSpace::getIndexOfBindingIndex(const std::vector<PipelineResourceBinding>& bindings, uint32_t bindingIndex)
{
	const uint32_t numBindings = static_cast<uint32_t>(bindings.size());
	for (uint32_t vectorIndex = 0; vectorIndex < numBindings; vectorIndex++)
	{
		if (bindings.at(vectorIndex).bindingIndex == bindingIndex)
		{
			return vectorIndex;
		}
	}

	return UINT_MAX;
}
//=============================================================================
#endif // RENDER_D3D12