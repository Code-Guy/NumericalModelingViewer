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

uniform float lineWidth;
uniform vec4 lineColor;

uniform float minTotalDeformation;
uniform float maxTotalDeformation;

uniform vec3 minDeformation;
uniform vec3 maxDeformation;

uniform vec3 minNormalElasticStrain;
uniform vec3 maxNormalElasticStrain;

uniform vec3 minShearElasticStrain;
uniform vec3 maxShearElasticStrain;

uniform float minMaximumPrincipalStress;
uniform float maxMaximumPrincipalStress;

uniform float minMiddlePrincipalStress;
uniform float maxMiddlePrincipalStress;

uniform float minMinimumPrincipalStress;
uniform float maxMinimumPrincipalStress;

uniform vec3 minNormalStress;
uniform vec3 maxNormalStress;

uniform vec3 minShearStress;
uniform vec3 maxShearStress;

uniform vec3 planeOrigin;
uniform vec3 planeNormal;

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

bool isOnFrontSideOfPlane(vec3 point)
{
	return dot(planeNormal, point) - dot(planeNormal, planeOrigin) > 0;
}

void main()
{
	if (!isOnFrontSideOfPlane(GPosition))
	{
		discard;
		return;
	}

	vec4 shadeColor = vec4(calcHeatmapColor(GTotalDeformation, minTotalDeformation, maxTotalDeformation), 1.0);

	float d = min(GEdgeDistance.x, GEdgeDistance.y);
	d = min(d, GEdgeDistance.z);

	float mixVal = smoothstep(lineWidth - 1, lineWidth + 1, d);
	FColor = mix(lineColor, shadeColor, mixVal);

	// gamma correction
	//float gamma = 2.2;
	//FColor.rgb = pow(FColor.rgb, vec3(1.0 / gamma));
}