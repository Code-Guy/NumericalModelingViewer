#version 420

layout(location = 0) in vec3 position;

out vec4 VColor;
out vec3 VPosition;

uniform mat4 mvp;

void main()
{
	gl_Position = mvp * vec4(position, 1.0);
	VColor = vec4(0.0, 0.0, 0.0, 1.0);
	VPosition = position;
}