#version 460 core

layout( push_constant ) uniform PerFrameData {
	mat4 mvp;
};

layout( location = 0 ) in vec3 pos;
layout( location = 0 ) out vec3 col;

void main() {
	gl_Position = mvp * vec4( pos, 1.0 );
	col         = pos.xzy;
}