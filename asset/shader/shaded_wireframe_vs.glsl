#version 330

layout(location = 0) in vec3 position;
layout(location = 1) in float totalDeformation;
layout(location = 2) in vec3 deformation;
layout(location = 3) in vec3 normalElasticStrain;
layout(location = 4) in vec3 shearElasticStrain;
layout(location = 5) in float maximumPrincipalStress;
layout(location = 6) in float middlePrincipalStress;
layout(location = 7) in float minimumPrincipalStress;
layout(location = 8) in vec3 normalStress;
layout(location = 9) in vec3 shearStress;

out float VTotalDeformation;
out vec3 VDeformation;
out vec3 VNormalElasticStrain;
out vec3 VShearElasticStrain;
out float VMaximumPrincipalStress;
out float VMiddlePrincipalStress;
out float VMinimumPrincipalStress;
out vec3 VNormalStress;
out vec3 VShearStress;

uniform mat4 mv;
uniform mat4 mvp;

void main()
{
	VTotalDeformation = totalDeformation;
	VDeformation = deformation;
	VNormalElasticStrain = normalElasticStrain;
	VShearElasticStrain = shearElasticStrain;
	VMaximumPrincipalStress = maximumPrincipalStress;
	VMiddlePrincipalStress = middlePrincipalStress;
	VMinimumPrincipalStress = minimumPrincipalStress;
	VNormalStress = normalStress;
	VShearStress = shearStress;

	gl_Position = mvp * vec4(position, 1.0);
}