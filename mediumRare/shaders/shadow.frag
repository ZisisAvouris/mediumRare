#include <../shaders/common.sp>
#include <../../data/shaders/AlphaTest.sp>
#include <../../data/shaders/UtilsPBR.sp>

layout( location = 0 ) in vec2 uv;
layout( location = 1 ) in flat uint materialId;

void main() {
	const float alphaCutoff = pc.materials.material[materialId].emissiveFactorAlphaCutoff.w;
	if ( alphaCutoff > 0.5 ) {
		discard;
	}
}
