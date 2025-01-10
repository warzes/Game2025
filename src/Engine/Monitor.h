#pragma once

struct DisplayChromaticities // https://en.wikipedia.org/wiki/Chromaticity
{
	DisplayChromaticities() = default;
	DisplayChromaticities(float rx, float ry, float gx, float gy, float bx, float by, float wx, float wy)
		: RedPrimary_xy{ rx, ry }
		, GreenPrimary_xy{ gx, gy }
		, BluePrimary_xy{ bx, by }
		, WhitePoint_xy{ wx, wy }
	{
	}
	DisplayChromaticities(const DXGI_OUTPUT_DESC1& d)
		: RedPrimary_xy{ d.RedPrimary[0]  , d.RedPrimary[1] }
		, GreenPrimary_xy{ d.GreenPrimary[0], d.GreenPrimary[1] }
		, BluePrimary_xy{ d.BluePrimary[0] , d.BluePrimary[1] }
		, WhitePoint_xy{ d.WhitePoint[0]  , d.WhitePoint[1] }
	{
	}

	// XY space coordinates of RGB primaries
	std::array<float, 2> RedPrimary_xy;
	std::array<float, 2> GreenPrimary_xy;
	std::array<float, 2> BluePrimary_xy;
	std::array<float, 2> WhitePoint_xy;
};