#version 420

layout( triangles ) in;
layout( triangle_strip, max_vertices = 3 ) out;

in vec3 VPosition[];
in float VTotalDeformation[];
in vec3 VDeformation[];
in vec3 VNormalElasticStrain[];
in vec3 VShearElasticStrain[];
in float VMaximumPrincipalStress[];
in float VMiddlePrincipalStress[];
in float VMinimumPrincipalStress[];
in vec3 VNormalStress[];
in vec3 VShearStress[];

out vec3 GPosition;
out float GTotalDeformation;
out vec3 GDeformation;
out vec3 GNormalElasticStrain;
out vec3 GShearElasticStrain;
out float GMaximumPrincipalStress;
out float GMiddlePrincipalStress;
out float GMinimumPrincipalStress;
out vec3 GNormalStress;
out vec3 GShearStress;

noperspective out vec3 GEdgeDistance;

uniform mat4 viewport;

void main()
{
	// Transform each vertex into viewport space
	vec3 p0 = vec3(viewport * (gl_in[0].gl_Position / gl_in[0].gl_Position.w));
	vec3 p1 = vec3(viewport * (gl_in[1].gl_Position / gl_in[1].gl_Position.w));
	vec3 p2 = vec3(viewport * (gl_in[2].gl_Position / gl_in[2].gl_Position.w));

	// Find the altitudes (ha, hb and hc)
	float a = length(p1 - p2);
	float b = length(p2 - p0);
	float c = length(p1 - p0);
	float alpha = acos( (b*b + c*c - a*a) / (2.0*b*c) );
	float beta = acos( (a*a + c*c - b*b) / (2.0*a*c) );
	float ha = abs( c * sin( beta ) );
	float hb = abs( c * sin( alpha ) );
	float hc = abs( b * sin( alpha ) );

	// Send the triangle along with the edge distances
	GPosition = VPosition[0];
	GTotalDeformation = VTotalDeformation[0];
	GDeformation = VDeformation[0];
	GNormalElasticStrain = VNormalElasticStrain[0];
	GShearElasticStrain = VShearElasticStrain[0];
	GMaximumPrincipalStress = VMaximumPrincipalStress[0];
	GMiddlePrincipalStress = VMiddlePrincipalStress[0];
	GMinimumPrincipalStress = VMinimumPrincipalStress[0];
	GNormalStress = VNormalStress[0];
	GShearStress = VShearStress[0];
	GEdgeDistance = vec3( ha, 0, 0 );
	gl_Position = gl_in[0].gl_Position;
	EmitVertex();

	GPosition = VPosition[1];
	GTotalDeformation = VTotalDeformation[1];
	GDeformation = VDeformation[1];
	GNormalElasticStrain = VNormalElasticStrain[1];
	GShearElasticStrain = VShearElasticStrain[1];
	GMaximumPrincipalStress = VMaximumPrincipalStress[1];
	GMiddlePrincipalStress = VMiddlePrincipalStress[1];
	GMinimumPrincipalStress = VMinimumPrincipalStress[1];
	GNormalStress = VNormalStress[1];
	GShearStress = VShearStress[1];
	GEdgeDistance = vec3( 0, hb, 0 );
	gl_Position = gl_in[1].gl_Position;
	EmitVertex();

	GPosition = VPosition[2];
	GTotalDeformation = VTotalDeformation[2];
	GDeformation = VDeformation[2];
	GNormalElasticStrain = VNormalElasticStrain[2];
	GShearElasticStrain = VShearElasticStrain[2];
	GMaximumPrincipalStress = VMaximumPrincipalStress[2];
	GMiddlePrincipalStress = VMiddlePrincipalStress[2];
	GMinimumPrincipalStress = VMinimumPrincipalStress[2];
	GNormalStress = VNormalStress[2];
	GShearStress = VShearStress[2];
	GEdgeDistance = vec3( 0, 0, hc );
	gl_Position = gl_in[2].gl_Position;
	EmitVertex();
	
	EndPrimitive();
}