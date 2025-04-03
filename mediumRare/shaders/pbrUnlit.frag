
layout( push_constant ) uniform PerFrameData {
	mat4 mvp;
	vec4 baseColor;
	uint textureId;
};

layout( location = 0 ) in vec2 uv;
layout( location = 1 ) in vec4 vertexColor;

layout( location = 0 ) out vec4 out_FragColor;

void main() {
	out_FragColor = textureBindless2D( textureId, 0, uv ) * vertexColor;
}
