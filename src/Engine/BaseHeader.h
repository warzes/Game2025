#pragma once

#include "EngineConfigMacros.h"

#if defined(_MSC_VER)
#	pragma warning(push, 3)
//#	pragma warning(disable : 4061)
#endif

#define _USE_MATH_DEFINES

#include <cassert>
#include <cstdint>
#include <cmath>

#include <optional>
#include <memory> // TODO: remove

#include <string>
#include <string_view>
//#include <array>
//#include <vector>
//#include <unordered_map>

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
#	define NOCTLMGR
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
#endif // PLATFORM_WINDOWS

#define GLM_ENABLE_EXPERIMENTAL 1
#include <glm/glm.hpp>
//#include <glm/ext.hpp>
//#include <glm/gtx/fast_trigonometry.hpp>

#if defined(_MSC_VER)
#	pragma warning(pop)
#endif