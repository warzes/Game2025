#pragma once

#if RENDER_D3D12

constexpr uint32_t    NUM_BACK_BUFFERS = 3;

struct RenderFeatures final
{
	bool allowTearing{ false };
};

const std::string DXErrorToStr(HRESULT hr);
const std::string ConvertStr(D3D_FEATURE_LEVEL level);

#endif // RENDER_D3D12