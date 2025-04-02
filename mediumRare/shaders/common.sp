
struct Vertex {
	float x, y, z;
	float u, v;
};

layout( std430, buffer_reference ) readonly buffer Vertices {
	Vertex in_Vertices[];
};

layout( std430, buffer_reference ) readonly buffer PerFrameData {
	mat4 model;
	mat4 view;
	mat4 proj;
	vec4 cameraPos;
	uint texture;
	float tesselationScale;
	Vertices vtx;
};

layout( push_constant ) uniform PushConstants {
	PerFrameData pc;
};

vec3 getPosition( int i ) {
	return vec3( pc.vtx.in_Vertices[i].x, pc.vtx.in_Vertices[i].y, pc.vtx.in_Vertices[i].z );
}
vec2 getTexCoords( int i ) {
	return vec2( pc.vtx.in_Vertices[i].u, pc.vtx.in_Vertices[i].v );
}
