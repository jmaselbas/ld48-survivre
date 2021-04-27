#version 300 es

in vec3 in_pos;
in vec3 in_normal;
in vec2 in_texcoord;
out vec3 normal;
out vec3 position;
out vec2 texcoord;
uniform mat4 proj;
uniform mat4 view;
uniform mat4 model;
uniform float time;

void main(void)
{
	gl_Position = proj * view * model * (vec4(in_pos, 1.0));
	texcoord = in_texcoord;
	normal = in_normal;
	position = vec3(model * (vec4(in_pos, 1.0)));
}
