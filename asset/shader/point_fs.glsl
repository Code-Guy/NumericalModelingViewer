#version 330

in vec3 VPosition;
in float VValue;

out vec4 FColor;

struct ValueRange
{
	float minValue;
	float maxValue;
};

uniform ValueRange valueRange;

const vec3 heatmapColors[5] = vec3[5](
	vec3(0, 0, 1), vec3(0, 1, 1), vec3(0, 1, 0), vec3(1, 1, 0), vec3(1, 0, 0)
);

vec3 calcHeatmapColor(float val, float minVal, float maxVal)
{
	val = clamp(val, minVal, maxVal);
	float i = (val - minVal) / (maxVal - minVal) * 4.0;
	int low = clamp(int(floor(i)), 0, 4);
	int high = clamp(int(ceil(i)), 0, 4);

	float t = i - low;
	return mix(heatmapColors[low], heatmapColors[high], t);
}

void main()
{
	// float thickness = 0.0001f;
	// float isoValue = 0.02f;
	// if (VValue > isoValue + thickness || VValue < isoValue - thickness )
	// {
	// 	discard;
	// 	return;
	// }

	FColor = vec4(calcHeatmapColor(VValue, valueRange.minValue, valueRange.maxValue), 1.0);
}