#pragma once

#include "EngineConfigMacros.h"

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

#define _USE_MATH_DEFINES

#include <cassert>
#include <cstdint>
#include <cmath>

#include <numeric>
#include <optional>
#include <memory> // TODO: remove

#include <mutex>

#include <algorithm>
#include <string>
#include <string_view>
#include <array>
#include <queue>
#include <vector>
#include <unordered_map>

#if PLATFORM_WINDOWS

#	define VC_EXTRALEAN
#	define WIN32_LEAN_AND_MEAN
#	define NOMINMAX
#	define NOBITMAP
#	define NOGDICAPMASKS
//#	define NOSYSMETRICS
#	define NOMENUS
#	define NOICONS
#	define NOSYSCOMMANDS
#	define NORASTEROPS
#	define OEMRESOURCE
#	define NOATOM
#	define NOCLIPBOARD
//#	define NOCTLMGR
#	define NODRAWTEXT
#	define NOKERNEL
//#	define NONLS
#	define NOMEMMGR
#	define NOMETAFILE
#	define NOOPENFILE
#	define NOSCROLL
#	define NOSERVICE
#	define NOSOUND
#	define NOTEXTMETRIC
#	define NOWH
#	define NOCOMM
#	define NOKANJI
#	define NOHELP
#	define NOPROFILER
#	define NODEFERWINDOWPOS
#	define NOMCX
#	define NORPC
#	define NOPROXYSTUB
#	define NOIMAGE
#	define NOTAPE
#	define NOGDI

#	include <winsdkver.h>
//#	define _WIN32_WINNT 0x0601 // Windows 7
#	define _WIN32_WINNT 0x0A00 // Windows 10
#	include <sdkddkver.h>
#	include <Windows.h>

#	if defined(CreateWindow)
#		undef CreateWindow
#	endif

#endif // PLATFORM_WINDOWS

#if RENDER_D3D12

#	include <wrl/client.h>
#	include <wrl/event.h>

#	include <d3dcompiler.h>
#	include <d3d12.h>
#	include <dxgi1_6.h>
#	if defined(_DEBUG)
#		include <dxgidebug.h>
#	endif

#	include <d3dx12.h>
#	include <D3D12MemAlloc.h>
#	include <DirectXShaderCompiler/dxcapi.h>
#	include <DirectXTex/DirectXTex.h>

#	include <DirectXMath.h>

#	include <pix3.h>

using Microsoft::WRL::ComPtr;
#endif // RENDER_D3D12

#if RENDER_VULKAN
#endif // RENDER_VULKAN

#if RENDER_WEBGPU
#endif // RENDER_WEBGPU

#define GLM_ENABLE_EXPERIMENTAL 1
#include <glm/glm.hpp>
#include <glm/ext.hpp>
//#include <glm/gtx/fast_trigonometry.hpp>

#if defined(_MSC_VER)
#	pragma warning(pop)
#endif