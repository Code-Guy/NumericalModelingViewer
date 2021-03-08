#version 330

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 texcoord;

out vec3 VTexcoord;

uniform mat4 mvp;

void main()
{
	VTexcoord = position;
	gl_Position = mvp * vec4(position, 1.0);
}