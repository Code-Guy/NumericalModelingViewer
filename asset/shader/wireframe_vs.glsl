#version 330

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 displacement;
layout(location = 2) in vec3 Sig123;
layout(location = 3) in vec4 SigXYZS;

out vec4 fragColor;

uniform mat4 mvp;

void main()
{
	gl_Position = mvp * vec4(position, 1.0);
	fragColor = vec4(1.0, 0.0, 0.0, 1.0);
}