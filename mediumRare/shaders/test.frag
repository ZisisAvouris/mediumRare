#version 460 core

layout ( location = 0 ) in vec3 col;
layout ( location = 1 ) in vec3 bary;

layout ( location = 0 ) out vec4 out_FragColor;

float edgeFactor( float thickness ) {
	vec3 a3 = smoothstep( vec3(0.0), fwidth(bary) * thickness, bary );
	return min( min( a3.x, a3.y ), a3.z );
}

void main() {
	out_FragColor = vec4( mix( vec3(0.0), col, edgeFactor(1.0) ), 1.0 );
}
