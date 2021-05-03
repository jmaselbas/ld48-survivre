#ifndef ENGINE_H
#define ENGINE_H

#include <stddef.h>

#include <plat/glad.h>

#include <math.h>

#include "util.h"
#include "math.h"

#include "mesh.h"
#include "camera.h"

#include "ring_buffer.h"

#include "wav.h"
#include "sampler.h"
struct shader {
	GLuint prog;
	GLuint vert;
	GLuint frag;
	GLuint geom;
};
GLint shader_load(struct shader *s, const char *vert, const char *frag, const char *geom);
GLint shader_reload(struct shader *s, const char *vert, const char *frag, const char *geom);
void shader_free(struct shader *s);

#endif
