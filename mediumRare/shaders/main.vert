
#include <../shaders/common.sp>

layout( location = 0 ) out vec2 uv_in;
layout( location = 1 ) out vec3 worldPos_in;

out gl_PerVertex {
	vec4 gl_Position;
};

void main() {
	vec4 pos    = vec4( getPosition( gl_VertexIndex ), 1.0 );
	gl_Position = pc.proj * pc.view * pc.model * pos;
	
	uv_in       = getTexCoords( gl_VertexIndex );
	worldPos_in = ( pc.model * pos ).xyz;
}
