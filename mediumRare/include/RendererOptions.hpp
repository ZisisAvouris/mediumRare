#pragma once

namespace mr {
	enum RendererOption : unsigned int {
		Grid = 0,
		Wireframe,
		Skybox,
		BoundingBox,
		LightFrustum,

		MAX
	};
}
