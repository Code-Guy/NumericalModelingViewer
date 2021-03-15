#version 420

in vec3 GPosition;
in float GTotalDeformation;
in vec3 GDeformation;
in vec3 GNormalElasticStrain;
in vec3 GShearElasticStrain;
in float GMaximumPrincipalStress;
in float GMiddlePrincipalStress;
in float GMinimumPrincipalStress;
in vec3 GNormalStress;
in vec3 GShearStress;
noperspective in vec3 GEdgeDistance;

out vec4 FColor;

struct Line	
{
	float width;
	vec4 color;
};

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

uniform Line line;
uniform Plane plane;
uniform ValueRange valueRange;

const vec3 heatmapColors[5] = vec3[5](
	vec3(0, 0, 1), vec3(0, 1, 1), vec3(0, 1, 0), vec3(1, 1, 0), vec3(1, 0, 0)
);

vec3 calcHeatmapColor(float val, float minVal, float maxVal)
{
	float i = (val - minVal) / (maxVal - minVal) * 5.0;
	int low = int(floor(i));
	int high = int(ceil(i));

	float t = i - low;
	return mix(heatmapColors[low], heatmapColors[high], t);
}

bool isOnPositiveSideOfPlane(vec3 point, float epsilon = 0.01f)
{
	return dot(plane.normal, point) > plane.dist + epsilon;
}

void main()
{
	if (!isOnPositiveSideOfPlane(GPosition, 1.0f))
	{
		discard;
		return;
	}

	//vec4 shadeColor = vec4(calcHeatmapColor(GTotalDeformation, valueRange.minTotalDeformation, valueRange.maxTotalDeformation), 1.0);
	vec4 shadeColor = vec4(1.0, 1.0, 1.0, 1.0);

	float minDist = min(GEdgeDistance.x, GEdgeDistance.y);
	minDist = min(minDist, GEdgeDistance.z);

	float mixVal = smoothstep(line.width - 1, line.width + 1, minDist);
	FColor = mix(line.color, shadeColor, mixVal);

	// gamma correction
	//float gamma = 2.2;
	//FColor.rgb = pow(FColor.rgb, vec3(1.0 / gamma));
}