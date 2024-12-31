#pragma once

#if RENDER_D3D12

constexpr uint32_t MAX_BACK_BUFFER_COUNT = 3;

struct RenderFeatures final
{
	bool allowTearing{ false };
};

const std::string DXErrorToStr(HRESULT hr);
const std::string ConvertToStr(D3D_FEATURE_LEVEL level);

#endif // RENDER_D3D12