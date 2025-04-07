#pragma once

namespace mr {
	enum RendererOption : unsigned int {
		Grid = 0,
		Wireframe,
		Skybox,
		BoundingBox,
		LightFrustum,
		NoAA,
		MSAAx2,
		MSAAx4,
		MSAAx8,
		MSAAx16,
		SSAO,
		BlurSSAO,

		MAX
	};

	static const char *RenderOptionToString( const RendererOption option) {
		switch( option ) {
		case RendererOption::Grid:			return "Grid";
		case RendererOption::Wireframe:		return "Wireframe";
		case RendererOption::Skybox:		return "Skybox";
		case RendererOption::BoundingBox:	return "BoundingBox";
		case RendererOption::LightFrustum:	return "LightFrustum";
		case RendererOption::NoAA:			return "NoAA";
		case RendererOption::MSAAx2:		return "MSAAx2";
		case RendererOption::MSAAx4:		return "MSAAx4";
		case RendererOption::MSAAx8:		return "MSAAx8";
		case RendererOption::MSAAx16:		return "MSAAx16";
		case RendererOption::MAX:			return "MAX";
		default:							return "Invalid";
		}
	}
}
