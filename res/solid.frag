#version 300 es
precision highp float;
precision highp int;

in vec3 normal;
in vec2 texcoord;
out vec4 out_color;

uniform vec3 color;

void main(void)
{
	out_color = vec4(color, 0.0);
}
