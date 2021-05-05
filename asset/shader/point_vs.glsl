#version 420

layout(location = 0) in vec3 position;
layout(location = 1) in float value;

uniform mat4 mvp;

out vec3 VPosition;
out float VValue;

void main()
{
	gl_Position = mvp * vec4(position, 1.0);
	gl_PointSize = 4;

	VPosition = position;
	VValue = value;
}