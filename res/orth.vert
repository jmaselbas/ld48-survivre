#version 300 es
in vec3 in_pos;
in vec3 in_normal;
in vec2 in_texcoord;
out vec3 normal;
out vec2 texcoord;
uniform mat4 model;
uniform vec2 v2Resolution;

void main(void)
{
	gl_Position = model * vec4(in_pos, 1.0);
	normal = in_normal;
	texcoord = in_texcoord;
}
