#version 300 es
precision highp float;
precision highp int;

in vec3 normal;
in vec3 position;
in vec2 texcoord;
out vec4 out_color;

const float fogdensity = 0.001;
const vec3 fogcolor = vec3(0.4, 0.6, 0.8);

const vec3 bot = vec3(0.40, 0.5, 0.47);
const vec3 top = vec3(0.10, 0.14, 0.2);

const vec3 sun = normalize(vec3(1, 0.75, 0));
uniform vec3 camp;
uniform float time;

const float PI = 3.141592;
const float Epsilon = 0.00001;

// GGX/Towbridge-Reitz normal distribution function.
// Uses Disney's reparametrization of alpha = roughness^2.
float ndfGGX(float cosLh, float roughness)
{
	float alpha   = roughness * roughness;
	float alphaSq = alpha * alpha;

	float denom = (cosLh * cosLh) * (alphaSq - 1.0) + 1.0;
	return alphaSq / (PI * denom * denom);
}

// Single term for separable Schlick-GGX below.
float gaSchlickG1(float cosTheta, float k)
{
	return cosTheta / (cosTheta * (1.0 - k) + k);
}

// Schlick-GGX approximation of geometric attenuation function using Smith's method.
float gaSchlickGGX(float cosLi, float cosLo, float roughness)
{
	float r = roughness + 1.0;
	float k = (r * r) / 8.0; // Epic suggests using this roughness remapping for analytic lights.
	return gaSchlickG1(cosLi, k) * gaSchlickG1(cosLo, k);
}

// Shlick's approximation of the Fresnel factor.
float mfresnelSchlick(float cosTheta)
{
	return pow(1.0 - cosTheta, 5.0);
}

vec3 mbrdf(vec3 col, float metalness, float roughness, vec3 l, vec3 n, vec3 v)
{
	// Half-vector between l and v.
	vec3 h = normalize(l + v);
	float cosLo = max(0.0, dot(n, v));
	float cosLi = max(0.0, dot(n, l));
	float cosLh = max(0.0, dot(n, h));

	/* fresnel term for direct lighting. */
	float f = mfresnelSchlick(max(0.0, dot(h, v)));
	/* normal distribution */
	float d = ndfGGX(cosLh, roughness);
	/* geometric attenuation */
	float g = max(0.0, gaSchlickGGX(cosLi, cosLo, roughness));

	float f0 = mix(0.1, 1.0 - f,  metalness);
	float spec = (f0 * d * g) / max(Epsilon, 4.0 * cosLi * cosLo);

	/* diffuse */
	vec3 diff = col * d * step(0.01, cosLi);
	return (diff + spec) *  cosLi;
}
void main(void)
{
	vec2 uv = texcoord;
	float y = max(0.0, dot(normalize(position), vec3(0,1,0)));
	y = uv.x;
	vec3 col = mix(bot, top, y);
	float rof = mix(0.99, 0.8, y);
	float z = gl_FragCoord.z / gl_FragCoord.w;
	vec3 vdir = normalize(camp - position);

	col = 0.5 * col + mbrdf(col, 0.1, rof, normalize(sun), normalize(normal), vdir);

	vec3 v = normalize(camp - position);
	float yy = 1.0 - dot(normalize(vec3(0.0,1.0,0.0)), v);
	float den = fogdensity * mix(0.01, 1.0, clamp(yy,0.0,1.0));
	float fog = exp(-den* z * z);
	col = mix(fogcolor, col, clamp(fog, 0.0, 1.0));

	/* TODO: investigate dithering */
	out_color = vec4(col, 1.0);
}
