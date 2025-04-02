#include <../shaders/common.sp>

layout( vertices = 3 ) out;

layout( location = 0 ) in vec2 uv_in[];
layout( location = 1 ) in vec3 worldPos_in[];

in gl_PerVertex {
	vec4 gl_Position;
} gl_in[];

out gl_PerVertex {
	vec4 gl_Position;
} gl_out[];

struct vertex {
	vec2 uv;
};

layout( location = 0 ) out vertex Out[];

float getTesselationLevel( float dist0, float dist1 ) {
	const float distScale1 = 1.2;
	const float distScale2 = 1.7;
	const float avgDist    = ( dist0 + dist1 ) / ( 2.0 * pc.tesselationScale );

	if ( avgDist <= distScale1 ) return 5.0;
	if ( avgDist <= distScale2 ) return 3.0;

	return 1.0;
}

void main() {
	gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;
	Out[gl_InvocationID].uv             = uv_in[gl_InvocationID];

	vec3 c = pc.cameraPos.xyz;

	float eyeToVtxDist0 = distance( c, worldPos_in[0] );
	float eyeToVtxDist1 = distance( c, worldPos_in[1] );
	float eyeToVtxDist2 = distance( c, worldPos_in[2] );

	gl_TessLevelOuter[0] = getTesselationLevel( eyeToVtxDist0, eyeToVtxDist2 );
	gl_TessLevelOuter[1] = getTesselationLevel( eyeToVtxDist2, eyeToVtxDist0 );
	gl_TessLevelOuter[2] = getTesselationLevel( eyeToVtxDist0, eyeToVtxDist1 );
	gl_TessLevelOuter[0] = gl_TessLevelOuter[2];
}
