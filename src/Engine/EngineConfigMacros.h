#pragma once

// Platform
#define PLATFORM_ANDROID 0
#define PLATFORM_LINUX 0
#define PLATFORM_WINDOWS 0
#define PLATFORM_EMSCRIPTEN 0

// RenderAPI
#define RENDER_D3D12 1
#define RENDER_VULKAN 0
#define RENDER_WEBGPU 0

#if defined(__ANDROID__) || defined(__android__) || defined(ANDROID) || defined(__ANDROID_API__)
#	undef PLATFORM_ANDROID
#	define PLATFORM_ANDROID 1
#elif defined(__linux__)
#	undef PLATFORM_LINUX
#	define PLATFORM_LINUX 1
#elif defined(_WIN32) || defined(_WIN64)
#	undef PLATFORM_WINDOWS
#	define PLATFORM_WINDOWS 1
#elif defined(_EMSCRIPTEN)
#	undef PLATFORM_EMSCRIPTEN
#	define PLATFORM_EMSCRIPTEN 1
#endif

#if defined(_DEBUG)
#	define RHI_VALIDATION_ENABLED 1
#else
#	define RHI_VALIDATION_ENABLED 0
#endif

#define ENABLE_UNIT_TEST 1