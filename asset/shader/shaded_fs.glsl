#version 420

in vec3 VPosition;
in float VTotalDeformation;
in vec3 VDeformation;
in vec3 VNormalElasticStrain;
in vec3 VShearElasticStrain;
in float VMaximumPrincipalStress;
in float VMiddlePrincipalStress;
in float VMinimumPrincipalStress;
in vec3 VNormalStress;
in vec3 VShearStress;

out vec4 FColor;

struct Plane
{
	vec3 normal;
	float dist;
};

struct ValueRange
{
	float minTotalDeformation;
	float maxTotalDeformation;

	vec3 minDeformation;
	vec3 maxDeformation;

	vec3 minNormalElasticStrain;
	vec3 maxNormalElasticStrain;

	vec3 minShearElasticStrain;
	vec3 maxShearElasticStrain;

	float minMaximumPrincipalStress;
	float maxMaximumPrincipalStress;

	float minMiddlePrincipalStress;
	float maxMiddlePrincipalStress;

	float minMinimumPrincipalStress;
	float maxMinimumPrincipalStress;

	vec3 minNormalStress;
	vec3 maxNormalStress;

	vec3 minShearStress;
	vec3 maxShearStress;
};

uniform Plane plane;
uniform ValueRange valueRange;
uniform bool skipClip;

const vec3 heatmapColors[5] = vec3[5](
	vec3(0, 0, 1), vec3(0, 1, 1), vec3(0, 1, 0), vec3(1, 1, 0), vec3(1, 0, 0)
);

vec3 calcHeatmapColor(float val, float minVal, float maxVal)
{
	float i = (val - minVal) / (maxVal - minVal) * 4.0;
	int low = clamp(int(floor(i)), 0, 4);
	int high = clamp(int(ceil(i)), 0, 4);

	float t = i - low;
	return mix(heatmapColors[low], heatmapColors[high], t);
}

bool isOnPositiveSideOfPlane(vec3 point, float epsilon = 0.01f)
{
	return dot(plane.normal, point) > plane.dist + epsilon;
}

void main()
{
	if (!skipClip && !isOnPositiveSideOfPlane(VPosition, 1.0f))
	{
		discard;
		return;
	}

	float thickness = 0.003f;
	float isoValue = 0.02f;
	if (VTotalDeformation > isoValue + thickness || VTotalDeformation < isoValue - thickness )
	{
		discard;
		return;
	}

	FColor = vec4(calcHeatmapColor(VTotalDeformation, valueRange.minTotalDeformation, valueRange.maxTotalDeformation), 1.0);
	//FColor = vec4(1.0, 1.0, 1.0, 1.0);

	// gamma correction
	//float gamma = 2.2;
	//FColor.rgb = pow(FColor.rgb, vec3(1.0 / gamma));
}