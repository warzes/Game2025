#include "stdafx.h"
#if RENDER_D3D12
#include "HelperD3D12.h"
//=============================================================================
bool CreateShaderResourceView(ComPtr<ID3D12Device14> device, ComPtr<ID3D12Resource> tex, D3D12_CPU_DESCRIPTOR_HANDLE srvDescriptor, bool isCubeMap)
{
	const auto desc = tex->GetDesc();

	if ((desc.Flags & D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE) != 0)
	{
		Fatal("CreateShaderResourceView called on a resource created without support for SRV.");
		return false;
	}

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = desc.Format;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

	const UINT mipLevels = (desc.MipLevels) ? static_cast<UINT>(desc.MipLevels) : static_cast<UINT>(-1);

	switch (desc.Dimension)
	{
	case D3D12_RESOURCE_DIMENSION_TEXTURE1D:
		if (desc.DepthOrArraySize > 1)
		{
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1DARRAY;
			srvDesc.Texture1DArray.MipLevels = mipLevels;
			srvDesc.Texture1DArray.ArraySize = static_cast<UINT>(desc.DepthOrArraySize);
		}
		else
		{
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1D;
			srvDesc.Texture1D.MipLevels = mipLevels;
		}
		break;

	case D3D12_RESOURCE_DIMENSION_TEXTURE2D:
		if (isCubeMap)
		{
			if (desc.DepthOrArraySize > 6)
			{
				srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBEARRAY;
				srvDesc.TextureCubeArray.MipLevels = mipLevels;
				srvDesc.TextureCubeArray.NumCubes = static_cast<UINT>(desc.DepthOrArraySize / 6);
			}
			else
			{
				srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
				srvDesc.TextureCube.MipLevels = mipLevels;
			}
		}
		else if (desc.DepthOrArraySize > 1)
		{
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
			srvDesc.Texture2DArray.MipLevels = mipLevels;
			srvDesc.Texture2DArray.ArraySize = static_cast<UINT>(desc.DepthOrArraySize);
		}
		else
		{
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			srvDesc.Texture2D.MipLevels = mipLevels;
		}
		break;

	case D3D12_RESOURCE_DIMENSION_TEXTURE3D:
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
		srvDesc.Texture3D.MipLevels = mipLevels;
		break;

	case D3D12_RESOURCE_DIMENSION_BUFFER:
		Fatal("CreateShaderResourceView cannot be used with DIMENSION_BUFFER.\n\tUse CreateBufferShaderResourceView.");
		return false;

	case D3D12_RESOURCE_DIMENSION_UNKNOWN:
	default:
		Fatal("CreateShaderResourceView cannot be used with DIMENSION_UNKNOWN: " + std::to_string(desc.Dimension));
		return false;
	}

	device->CreateShaderResourceView(tex.Get(), &srvDesc, srvDescriptor);
	return true;
}
//=============================================================================
bool CreateUnorderedAccessView(ComPtr<ID3D12Device14> device, ComPtr<ID3D12Resource> tex, D3D12_CPU_DESCRIPTOR_HANDLE uavDescriptor, uint32_t mipLevel)
{
	const auto desc = tex->GetDesc();

	if ((desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS) == 0)
	{
		Fatal("CreateUnorderedResourceView called on a resource created without support for UAV.");
		return false;
	}

	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.Format = desc.Format;

	switch (desc.Dimension)
	{
	case D3D12_RESOURCE_DIMENSION_TEXTURE1D:
		if (desc.DepthOrArraySize > 1)
		{
			uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE1DARRAY;
			uavDesc.Texture1DArray.MipSlice = mipLevel;
			uavDesc.Texture1DArray.FirstArraySlice = 0;
			uavDesc.Texture1DArray.ArraySize = desc.DepthOrArraySize;
		}
		else
		{
			uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE1D;
			uavDesc.Texture1D.MipSlice = mipLevel;
		}
		break;

	case D3D12_RESOURCE_DIMENSION_TEXTURE2D:
		if (desc.DepthOrArraySize > 1)
		{
			uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
			uavDesc.Texture2DArray.MipSlice = mipLevel;
			uavDesc.Texture2DArray.ArraySize = desc.DepthOrArraySize;
		}
		else
		{
			uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
			uavDesc.Texture2D.MipSlice = mipLevel;
		}
		break;

	case D3D12_RESOURCE_DIMENSION_TEXTURE3D:
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
		uavDesc.Texture3D.MipSlice = mipLevel;
		uavDesc.Texture3D.WSize = desc.DepthOrArraySize;
		break;

	case D3D12_RESOURCE_DIMENSION_BUFFER:
		Fatal("CreateUnorderedResourceView cannot be used with DIMENSION_BUFFER.\n\tUse CreateBufferUnorderedAccessView.");
		return false;

	case D3D12_RESOURCE_DIMENSION_UNKNOWN:
	default:
		Fatal("CreateUnorderedResourceView cannot be used with DIMENSION_UNKNOWN: " + std::to_string(desc.Dimension));
		return false;
	}
	device->CreateUnorderedAccessView(tex.Get(), nullptr, &uavDesc, uavDescriptor);
	return true;
}
//=============================================================================
bool CreateRenderTargetView(ComPtr<ID3D12Device14> device, ComPtr<ID3D12Resource> tex, D3D12_CPU_DESCRIPTOR_HANDLE rtvDescriptor, uint32_t mipLevel)
{
	const auto desc = tex->GetDesc();

	if ((desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET) == 0)
	{
		Fatal("CreateRenderTargetView called on a resource created without support for RTV.");
		return false;
	}

	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	rtvDesc.Format = desc.Format;

	switch (desc.Dimension)
	{
	case D3D12_RESOURCE_DIMENSION_TEXTURE1D:
		if (desc.DepthOrArraySize > 1)
		{
			rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE1DARRAY;
			rtvDesc.Texture1DArray.MipSlice = mipLevel;
			rtvDesc.Texture1DArray.FirstArraySlice = 0;
			rtvDesc.Texture1DArray.ArraySize = desc.DepthOrArraySize;
		}
		else
		{
			rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE1D;
			rtvDesc.Texture1D.MipSlice = mipLevel;
		}
		break;

	case D3D12_RESOURCE_DIMENSION_TEXTURE2D:
		if (desc.SampleDesc.Count > 1)
		{
			if (desc.DepthOrArraySize > 1)
			{
				rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMSARRAY;
				rtvDesc.Texture2DMSArray.ArraySize = desc.DepthOrArraySize;
			}
			else
			{
				rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMS;
			}
		}
		else if (desc.DepthOrArraySize > 1)
		{
			rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
			rtvDesc.Texture2DArray.MipSlice = mipLevel;
			rtvDesc.Texture2DArray.ArraySize = desc.DepthOrArraySize;
		}
		else
		{
			rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
			rtvDesc.Texture2D.MipSlice = mipLevel;
		}
		break;

	case D3D12_RESOURCE_DIMENSION_TEXTURE3D:
		rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE3D;
		rtvDesc.Texture3D.MipSlice = mipLevel;
		rtvDesc.Texture3D.WSize = desc.DepthOrArraySize;
		break;

	case D3D12_RESOURCE_DIMENSION_BUFFER:
		Fatal("CreateRenderTargetView cannot be used with DIMENSION_BUFFER.");
		return false;

	case D3D12_RESOURCE_DIMENSION_UNKNOWN:
	default:
		Fatal("CreateRenderTargetView cannot be used with DIMENSION_UNKNOWN: " + std::to_string(desc.Dimension));
		return false;
	}
	device->CreateRenderTargetView(tex.Get(), &rtvDesc, rtvDescriptor);
	return true;
}
//=============================================================================
bool CreateBufferShaderResourceView(ComPtr<ID3D12Device14> device, ComPtr<ID3D12Resource> buffer, D3D12_CPU_DESCRIPTOR_HANDLE srvDescriptor, uint32_t stride)
{
	const auto desc = buffer->GetDesc();

	if (desc.Dimension != D3D12_RESOURCE_DIMENSION_BUFFER || (desc.Flags & D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE) != 0)
	{
		Fatal("CreateBufferShaderResourceView called on an unsupported resource.");
		return false;
	}

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format                          = DXGI_FORMAT_UNKNOWN;
	srvDesc.ViewDimension                   = D3D12_SRV_DIMENSION_BUFFER;
	srvDesc.Shader4ComponentMapping         = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Buffer.FirstElement             = 0;
	srvDesc.Buffer.NumElements              = (stride > 0)
										? static_cast<UINT>(desc.Width / stride)
										: static_cast<UINT>(desc.Width);
	srvDesc.Buffer.StructureByteStride      = stride;

	device->CreateShaderResourceView(buffer.Get(), &srvDesc, srvDescriptor);
	return true;
}
//=============================================================================
bool CreateBufferUnorderedAccessView(ComPtr<ID3D12Device14> device, ComPtr<ID3D12Resource> buffer, D3D12_CPU_DESCRIPTOR_HANDLE uavDescriptor, uint32_t stride, D3D12_BUFFER_UAV_FLAGS flag, uint32_t counterOffset, ComPtr<ID3D12Resource> counterResource)
{
	const auto desc = buffer->GetDesc();

	if (desc.Dimension != D3D12_RESOURCE_DIMENSION_BUFFER || (desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS) == 0)
	{
		Fatal("CreateBufferUnorderedAccessView called on an unsupported resource.");
		return false;
	}

	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.Format                           = DXGI_FORMAT_UNKNOWN;
	uavDesc.ViewDimension                    = D3D12_UAV_DIMENSION_BUFFER;
	uavDesc.Buffer.FirstElement              = 0;
	uavDesc.Buffer.NumElements               = (stride > 0)
										? static_cast<UINT>(desc.Width / stride)
										: static_cast<UINT>(desc.Width);
	uavDesc.Buffer.StructureByteStride       = stride;
	uavDesc.Buffer.CounterOffsetInBytes      = counterOffset;
	uavDesc.Buffer.Flags = flag;

	device->CreateUnorderedAccessView(buffer.Get(), counterResource.Get(), &uavDesc, uavDescriptor);
	return true;
}
//=============================================================================
#endif // RENDER_D3D12