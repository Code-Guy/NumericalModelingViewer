#version 420

struct Plane
{
	vec3 normal;
	float dist;
};

uniform Plane plane;
uniform bool skipClip;

in vec4 VColor;
in vec3 VPosition;

out vec4 FColor;

bool isOnPositiveSideOfPlane(vec3 point, float epsilon = 0.01f)
{
	return dot(plane.normal, point) > plane.dist + epsilon;
}

void main()
{
	// if (!skipClip && !isOnPositiveSideOfPlane(VPosition, 1.0f))
	// {
	// 	discard;
	// 	return;
	// }

	FColor = VColor;
}