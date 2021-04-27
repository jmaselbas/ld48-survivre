#version 300 es
precision highp float;
precision highp int;

in vec2 texcoord;

uniform float time;

out vec4 out_color;

#define PI 3.1416
#define PI2 (PI*2.0)

const vec3 bg = vec3(0.2, 0.2, 0.2);
const vec3 fg = vec3(0.4, 0.4, 0.4);

float abssin(float f) { return 0.5 + 0.5 * sin(f); }

mat2 rot2(float a) { return mat2(cos(a), -sin(a), sin(a), cos(a)); }

vec3 sline(vec2 uv)
{
	vec3 col = vec3(0);

	uv.y += time * 0.1;
	col = mix(bg, fg, fract(uv.y * 40.0));
	col *= mix(0.5, 1.0, abssin(time * 10.0));
	return col;
}

float ht(vec2 uv, float a, float v)
{
        uv *= 10.0;
	uv *= rot2(a);
	uv.x += 0.5 * floor(uv.y);
	return step(length(fract(uv) - 0.5), v / 2.0);
}

vec3 shader(void)
{
	vec3 col = vec3(0);
	vec2 uv = texcoord;
	vec2 sc = uv;
	vec2 pv = uv;
	vec2 tv = uv;
	vec2 cv = uv;
	vec2 dv = uv;
	float circle = length(cv) - time;
	float dist_2 = 1.5 * sin(time / 100.0) * tan(time * 0.5);
	for (float i = 0.0; i < 10.0; i++) {
		float a = PI2 * (i / 10.0);
		cv.x += dist_2 * sin(time + a);
		cv.y += dist_2 * cos(time + a);
		circle = min(circle, length(cv) - time);
	}

        float dist = 1.5;
	tv.x += 0.001 * tan(time);
	tv.x += dist * sin(time);
	tv.y += dist * cos(time);

        /* zoom */
	uv *= mix(0.5, 2.0, 0.5 + 0.5 * sin(time / 10.0));
	/* roto */
	uv *= rot2(0.0025 * time);

        float speed  = time / 10.0;
	uv.y += 0.05 * sin(uv.x * 1.0 * PI2 + speed);
	uv.y = fract(uv.y * 10.0);
	col.r = ht(uv, PI2 * sin(time / 10.0), 0.5 * uv.y);

        col.rgb = col.rrr;
	float lsize = 10.0 * (0.5 + 0.5 * sin(time / 10.0));
	col.rgb *= step(mix(0.4, 0.4, 0.5), fract(length(tv * lsize)));

        /* invert */
	if (fract(circle) < 0.5)
		col.rgb = col.rgb;
	else
		col.rgb = 1.0 - col.rrr;

        vec2 off;
	off.x += dist * sin(time / 10.0);
	off.y += dist * cos(time / 10.0);

        if (ht(sc + off, 0.0, length(sc)) > 0.5)
		col = 1.0 - col;

        return col;
}	

void main(void) {
	vec2 uv = texcoord;
	vec3 col = vec3(0.0);

	col = sline(uv);
	uv.y += time * 0.1;
	if (time < 5.0)
	   	 col = vec3(0.0);
	else if (time < 6.0)
	   	 col = vec3(1) * (time - 5.0) / 1.0;
	else if (time < 30.0)
		col = mix(vec3(1.0), col, uv.y*fract(1.7 * uv.y));
	else if (time < 60.0)
		col = mix(vec3(1.0), col, fract(2.7 * uv.y));
	else if (time < 90.0) {
		uv = fract(uv * 2.0);
		col = col*mix(1.0, 0.0, uv.y*fract(uv.y * uv.y));
	} else {
		col = shader();
	}

	out_color = vec4(col,0.0);
}
