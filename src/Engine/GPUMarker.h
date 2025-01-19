#pragma once

#define SCOPED_GPU_MARKER(pCmd, pStr)             ScopedGPUMarker GPUMarker(pCmd,pStr)

#define SCOPED_CPU_MARKER(pStr)                   ScopedMarker    CPUMarker(pStr)
#define SCOPED_CPU_MARKER_C(pStr, PIXColor)       ScopedMarker    CPUMarker(pStr, PIXColor)
#define SCOPED_CPU_MARKER_F(pStr, ...)            ScopedMarker    CPUMarker(PIX_COLOR_DEFAULT, pStr, __VA_ARGS__)
#define SCOPED_CPU_MARKER_CF(PIXColor, pStr, ...) ScopedMarker    CPUMarker(PIXColor, pStr, __VA_ARGS__)

class ScopedMarker
{
public:
	ScopedMarker(const char* pLabel, unsigned PIXColor = PIX_COLOR_DEFAULT);
	template<class ... Args>
	ScopedMarker(unsigned PIXColor, const char* pLabel, Args&&... args);
	~ScopedMarker();
};

template<class ...Args>
inline ScopedMarker::ScopedMarker(unsigned PIXColor, const char* pFormat, Args && ...args)
{
	char buf[256];
	sprintf_s(buf, pFormat, args...);
	PIXBeginEvent(PIXColor, buf);
}

class ScopedGPUMarker
{
public:
	ScopedGPUMarker(ID3D12GraphicsCommandList* pCmdList, const char* pLabel, unsigned PIXColor = PIX_COLOR_DEFAULT);
	ScopedGPUMarker(ID3D12CommandQueue* pCmdQueue, const char* pLabel, unsigned PIXColor = PIX_COLOR_DEFAULT);
	~ScopedGPUMarker();

	ScopedGPUMarker(const ScopedGPUMarker&) = delete;
	ScopedGPUMarker(ScopedGPUMarker&&) = delete;
	ScopedGPUMarker& operator=(ScopedGPUMarker&&) = delete;
private:
	union
	{
		ID3D12GraphicsCommandList* mpCmdList;
		ID3D12CommandQueue* mpCmdQueue;
	};
};

// TODO: совместить с ScopedGPUMarker
class ScopedPixEvent final
{
public:
	ScopedPixEvent(ComPtr<ID3D12GraphicsCommandList> commandList, PCWSTR pFormat) noexcept
		: m_commandList(commandList)
	{
		PIXBeginEvent(m_commandList.Get(), 0, pFormat);
	}
	~ScopedPixEvent()
	{
		PIXEndEvent(m_commandList.Get());
	}

private:
	ComPtr<ID3D12GraphicsCommandList> m_commandList;
};