#version 330

noperspective in vec4 GEdgeDistance;

out vec4 FColor;

uniform float LineWidth;
uniform vec4 LineColor;

void main() 
{
	vec4 shadeColor = vec4(1.0, 1.0, 1.0, 1.0);

	float d = min(GEdgeDistance.x, GEdgeDistance.y);
	d = min(d, GEdgeDistance.z);

	float mixVal;
	if ((d == GEdgeDistance.x && GEdgeDistance.w == 0) ||
		(d == GEdgeDistance.y && GEdgeDistance.w == 1) ||
		(d == GEdgeDistance.z && GEdgeDistance.w == 2))
	{
		mixVal = 1.0;
	}
	else
	{
		mixVal = smoothstep(LineWidth - 1, LineWidth + 1, d);
	}

	FColor = mix(LineColor, shadeColor, mixVal);
}