/***********************************************************
*   Simple GLSL demo code for COS426 Computer Graphics     *
*                           ****                           *
*   Most is borrowed from                                  *
*   http://www.lighthouse3d.com/opengl/glsl/               *
*                           ****                           *
*   Dependencies: glew, glut, gl                           *
*                           ****                           *
*   Pulled together by Aleksey Boyko(aboyko@princeton.edu) *
************************************************************/

varying vec3 normal;
varying vec2 texture_coordinate;
varying vec3 vertex_light_position1;
varying vec3 vertex_light_half_vector1;
varying vec3 vertex_light_position2;
varying vec3 vertex_light_half_vector2;

varying vec4 lightDir;



uniform vec3 _WorldSpaceCameraPos; // camera position in world space
uniform mat4 _Object2World; // model matrix	

void main()
{
//	ec_pos = vec3(gl_ModelViewMatrix * gl_Vertex);
//	ec_Pos = vec4(gl_ModelViewMatrix * gl_Vertex);
//	lightDir = normalize(vec3(gl_LightSource[0].position) - ecPos.xyz);

	normal = normalize(gl_NormalMatrix * gl_Normal);

	// Calculate the light position for this vertex
    	vertex_light_position1 = normalize(gl_LightSource[0].position.xyz);

	// Calculate the light’s half vector
    	vertex_light_half_vector1 = normalize(gl_LightSource[0].halfVector.xyz);

	// Calculate the light position for this vertex
    	vertex_light_position2 = normalize(gl_LightSource[1].position.xyz);

	// Calculate the light’s half vector
    	vertex_light_half_vector2 = normalize(gl_LightSource[1].halfVector.xyz);


	//gl_Position = ftransform();
	gl_FrontColor = gl_Color;
	gl_BackColor = vec4(0.0,0.0,0.0,1.0);

	gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;	
	//	texture_coordinate = vec2(gl_MultiTexCoord0);
	gl_TexCoord[0] = gl_MultiTexCoord0;  

//	vVertex = gl_Vertex.xyz;
} 