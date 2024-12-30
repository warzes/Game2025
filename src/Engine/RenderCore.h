#pragma once

struct RenderSystemCreateInfo final
{
	bool vsync{ false };
#if RENDER_D3D12
	bool useWarp{ false };

	struct Debug final
	{
		bool enableGPUBasedValidation{ true };
		bool enableSynchronizedCommandQueueValidation{ true };
		bool enableAutoName{ true };
	} debug;
#endif
};

inline uint32_t GetGroupCount(uint32_t threadCount, uint32_t groupSize)
{
	return (threadCount + groupSize - 1) / groupSize;
}

inline uint32_t AlignU32(uint32_t valueToAlign, uint32_t alignment)
{
	alignment -= 1;
	return (uint32_t)((valueToAlign + alignment) & ~alignment);
}

inline uint64_t AlignU64(uint64_t valueToAlign, uint64_t alignment)
{
	alignment -= 1;
	return (uint64_t)((valueToAlign + alignment) & ~alignment);
}
