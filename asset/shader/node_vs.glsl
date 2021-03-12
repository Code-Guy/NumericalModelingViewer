#version 420

layout(location = 0) in vec3 position;

uniform mat4 mvp;

void main()
{
	gl_Position = mvp * vec4(position, 1.0);
	gl_PointSize = 4;
}