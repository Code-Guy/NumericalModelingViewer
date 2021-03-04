#version 330

layout(location = 0) in vec3 position;

out vec4 fragColor;

uniform mat4 mvp;

void main()
{
	gl_Position = mvp * vec4(position, 1.0);
	fragColor = vec4(0.0, 0.0, 0.0, 1.0);
}