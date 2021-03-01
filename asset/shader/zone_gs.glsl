#version 330

layout( triangles ) in;
layout( triangle_strip, max_vertices = 3 ) out;

in vec3 VPosition[];

out vec3 GPosition;
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
	GEdgeDistance = vec3( ha, 0, 0 );
	gl_Position = gl_in[0].gl_Position;
	EmitVertex();

	GEdgeDistance = vec3( 0, hb, 0 );
	gl_Position = gl_in[1].gl_Position;
	EmitVertex();

	GEdgeDistance = vec3( 0, 0, hc );
	gl_Position = gl_in[2].gl_Position;
	EmitVertex();
	
	EndPrimitive();
}