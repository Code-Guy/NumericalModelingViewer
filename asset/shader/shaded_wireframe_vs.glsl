#version 330

layout(location = 0) in vec3 position;

out vec3 VPosition;

uniform mat4 mv;
uniform mat4 mvp;

void main()
{
	VPosition = position;
	gl_Position = mvp * vec4(position, 1.0);
}