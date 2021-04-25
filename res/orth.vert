#version 410 core
in vec3 in_pos;
in vec3 in_normal;
in vec2 in_texcoord;
out vec3 normal;
out vec2 texcoord;
uniform mat4 model;
uniform vec2 v2Resolution;

void main(void)
{
	float r = v2Resolution.x / v2Resolution.y;
	mat4 ratio = mat4(1.0, 0.0, 0.0, 0.0,
			  0.0,   r, 0.0, 0.0, 
			  0.0, 0.0, 1.0, 0.0,
			  0.0, 0.0, 0.0, 1.0);
	gl_Position = ratio * model * vec4(in_pos, 1.0);
	normal = in_normal;
	texcoord = in_texcoord;
}
