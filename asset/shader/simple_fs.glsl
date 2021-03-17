#version 420

struct Plane
{
	vec3 normal;
	float dist;
};

in vec4 VColor;
in vec3 VPosition;

out vec4 FColor;

uniform Plane plane;

bool isOnPositiveSideOfPlane(vec3 point, float epsilon = 0.01f)
{
	return dot(plane.normal, point) > plane.dist + epsilon;
}

void main()
{
	if (!isOnPositiveSideOfPlane(VPosition, 0.0f))
	{
		discard;
		return;
	}

	FColor = VColor;
}