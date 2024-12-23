#pragma once

#if defined(_ENGINE_PRIVATE_HEADER)

#if defined(_MSC_VER)
#	pragma warning(push, 3)
#	pragma warning(disable : 4265)
#	pragma warning(disable : 4365)
#	pragma warning(disable : 4625)
#	pragma warning(disable : 4626)
#	pragma warning(disable : 4820)
#	pragma warning(disable : 5039)
#	pragma warning(disable : 5204)
#	pragma warning(disable : 5220)
#endif

#if RENDER_D3D12

#	include <d3dcompiler.h>
#	include <d3d12.h>
#	include <dxgi1_6.h>
//#	include <dxgidebug.h>

#	include <wrl.h>
using Microsoft::WRL::ComPtr;

#endif // RENDER_D3D12

#if RENDER_VULKAN
#endif // RENDER_VULKAN

#if RENDER_WEBGPU
#endif // RENDER_WEBGPU

#if defined(_MSC_VER)
#	pragma warning(pop)
#endif

#endif // _ENGINE_PRIVATE_HEADER