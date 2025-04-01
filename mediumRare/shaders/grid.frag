#version 460 core
#include <../shaders/gridParams.h>

vec4 gridColor( vec2 uv, vec2 camPos ) {
	vec2 dudv = vec2( length( vec2(dFdx(uv.x), dFdy(uv.x) ) ), length( vec2(dFdx(uv.y), dFdy(uv.y)) ) );

	float lodLevel = max( 0.0, log10( (length(dudv) * gridMinPixelsBetweenCells) /gridCellSize ) + 1.0 );
	float lodFade  = fract( lodLevel );

	float lod0 = gridCellSize * pow( 10.0, floor(lodLevel) );
	float lod1 = lod0 * 10.0;
	float lod2 = lod1 * 10.0;

	dudv *= 4.0;

	uv += dudv * 0.5;

	float lod0a = maxVec2( vec2(1.0) - abs(satVec2(mod(uv, lod0) / dudv) * 2.0 - vec2(1.0)) );
	float lod1a = maxVec2( vec2(1.0) - abs(satVec2(mod(uv, lod1) / dudv) * 2.0 - vec2(1.0)) );
	float lod2a = maxVec2( vec2(1.0) - abs(satVec2(mod(uv, lod2) / dudv) * 2.0 - vec2(1.0)) );

	uv -= camPos;

	vec4 c = lod2a > 0.0 ? gridColorThick : lod1a > 0.0 ? mix( gridColorThick, gridColorThin, lodFade ) : gridColorThin;
	
	float opacityFalloff = 1.0 - satf( length(uv) / gridSize );
	c.a *= ( lod2a > 0.0 ? lod2a : lod1a > 0.0 ? lod1a : (lod0a * ( 1.0 - lodFade ) )) * opacityFalloff;
	return c;
}

layout( location = 0 ) in vec2 uv;
layout( location = 1 ) in vec2 cameraPos;

layout( location = 0 ) out vec4 out_FragColor;

void main() {
	out_FragColor = gridColor( uv, cameraPos );
}
