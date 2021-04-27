#include <stdio.h>
#include <string.h>

#include "engine.h"

static GLint
shader_compile(GLsizei count, const GLchar **string, const GLint *length, GLenum type, GLuint *out)
{
	char logbuf[1024];
	GLsizei logsize;
	GLuint shader = glCreateShader(type);
	GLint ret;

	glShaderSource(shader, count, string, length);
        glCompileShader(shader);

	glGetShaderiv(shader, GL_COMPILE_STATUS, &ret);
	if (ret != GL_TRUE) {
		glGetShaderInfoLog(shader, sizeof(logbuf), &logsize, logbuf);
		glDeleteShader(shader);
		fprintf(stderr, "--- ERROR ---\n%s", logbuf);
		return ret;
	}

	*out = shader;
	return GL_TRUE;
}

GLint
shader_reload(struct shader *s, const char *vert_src, const char *frag_src, const char *geom_src)
{
	char logbuf[1024];
	GLsizei logsize;
	GLuint prog;
	GLuint vert = s->vert;
	GLuint frag = s->frag;
	GLuint geom = s->geom;
	GLint vert_len;
	GLint frag_len;
	GLint geom_len;
	GLint ret;

	if (vert_src) {
		vert_len = strlen(vert_src);
		ret = shader_compile(1, &vert_src, &vert_len, GL_VERTEX_SHADER, &vert);
		if (ret != GL_TRUE)
			goto err_vert;
	}

	if (frag_src) {
		frag_len = strlen(frag_src);
		ret = shader_compile(1, &frag_src, &frag_len, GL_FRAGMENT_SHADER, &frag);
		if (ret != GL_TRUE)
			goto err_frag;
	}

#ifdef GL_GEOMETRY_SHADER
	if (geom_src) {
		geom_len = strlen(geom_src);
		ret = shader_compile(1, &geom_src, &geom_len, GL_GEOMETRY_SHADER, &geom);
		if (ret != GL_TRUE)
			goto err_geom;
	}
#endif

	/* Create a new program */
	prog = glCreateProgram();
	if (!prog)
		goto err_prog;
	if (vert)
		glAttachShader(prog, vert);
	if (frag)
		glAttachShader(prog, frag);
	if (geom)
		glAttachShader(prog, geom);

	glLinkProgram(prog);
	glGetProgramiv(prog, GL_LINK_STATUS, &ret);
	if (ret != GL_TRUE) {
		glGetProgramInfoLog(prog, sizeof(logbuf), &logsize, logbuf);
		fprintf(stderr, "--- ERROR ---\n%s", logbuf);
		goto err_link;
	}

	if (s->prog) {
		glDetachShader(s->prog, s->vert);
		glDetachShader(s->prog, s->frag);
		glDetachShader(s->prog, s->geom);
		glDeleteProgram(s->prog);
		if (s->vert != vert)
			glDeleteShader(s->vert);
		if (s->vert != frag)
			glDeleteShader(s->frag);
		if (s->geom != geom)
			glDeleteShader(s->geom);
	}
	s->prog = prog;
	s->vert = vert;
	s->frag = frag;
	s->frag = geom;
	return 0;

err_link:
	if (vert)
		glDetachShader(prog, vert);
	if (frag)
		glDetachShader(prog, frag);
	if (geom)
		glDetachShader(prog, geom);
	glDeleteProgram(prog);
err_prog:
	if (geom_src)
		glDeleteShader(geom);
err_geom:
	if (frag_src)
		glDeleteShader(frag);
err_frag:
	if (vert_src)
		glDeleteShader(vert);
err_vert:

	return ret;
}

GLint
shader_load(struct shader *s, const char *vert_src, const char *frag_src, const char *geom_src)
{
	return shader_reload(s, vert_src, frag_src, geom_src);
}

void
shader_free(struct shader *s)
{
	glDetachShader(s->prog, s->vert);
	glDetachShader(s->prog, s->frag);
	glDeleteShader(s->vert);
	glDeleteShader(s->frag);
	glDeleteProgram(s->prog);
	s->vert = 0;
	s->frag = 0;
	s->prog = 0;
}
