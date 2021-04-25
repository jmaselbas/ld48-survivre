#version 410 core
in vec3 normal;
in vec2 texcoord;
out vec4 out_color;

uniform vec3 color;

void main(void)
{
	out_color = vec4(color,0);
}
