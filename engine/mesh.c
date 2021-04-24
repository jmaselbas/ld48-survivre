#include "engine.h"

static struct bounding_volume
bounding_volume(size_t count, float *positions)
{
	struct bounding_volume bvol = { 0 };
	float len, max = 0;
	unsigned int i;
	vec3 pos;

	if (!positions)
		return bvol;

	for (i = 0; i < count; i++) {
		bvol.min.x = MIN(bvol.min.x, positions[i * 3 + 0]);
		bvol.min.y = MIN(bvol.min.y, positions[i * 3 + 1]);
		bvol.min.z = MIN(bvol.min.z, positions[i * 3 + 2]);
		bvol.max.x = MAX(bvol.max.x, positions[i * 3 + 0]);
		bvol.max.y = MAX(bvol.max.y, positions[i * 3 + 1]);
		bvol.max.z = MAX(bvol.max.z, positions[i * 3 + 2]);
	}

	/* full extent from one corner to the other */
	bvol.ext = vec3_mult(0.5, vec3_sub(bvol.max, bvol.min));
	/* half extent plus min is the offset from origin to center */
	bvol.off = vec3_add(bvol.ext, bvol.min);

	for (i = 0; i < count; i++) {
		pos.x = positions[i * 3 + 0];
		pos.y = positions[i * 3 + 1];
		pos.z = positions[i * 3 + 2];

		/* distance to the bounding volume's center */
		pos = vec3_sub(pos, bvol.off);
		len = vec3_dot(pos, pos);
		if (len > max)
			max = len;
	}
	bvol.radius = sqrt(max);

	return bvol;
}

void
mesh_load(struct mesh *m, size_t count, GLenum primitive, float *positions, float *normals, float *texcoords)
{
	GLuint vbo_count = 0;

	glGenVertexArrays(1, &m->vao);
	glBindVertexArray(m->vao);

	m->idx_positions = (positions) ? vbo_count++ : 0;
	m->idx_normals   = (normals)   ? vbo_count++ : 0;
	m->idx_texcoords = (texcoords) ? vbo_count++ : 0;

	glGenBuffers(vbo_count, m->vbo);

	m->vbo_count = vbo_count;
	m->vertex_count = count;
	m->index_count = 0;

	if (positions) {
		glBindBuffer(GL_ARRAY_BUFFER, m->vbo[m->idx_positions]);
		glBufferData(GL_ARRAY_BUFFER, m->vertex_count * 3 * sizeof(float), positions, GL_STATIC_DRAW);
	}

	if (normals) {
		glBindBuffer(GL_ARRAY_BUFFER, m->vbo[m->idx_normals]);
		glBufferData(GL_ARRAY_BUFFER, m->vertex_count * 3 * sizeof(float), normals, GL_STATIC_DRAW);
	}

	if (texcoords) {
		glBindBuffer(GL_ARRAY_BUFFER, m->vbo[m->idx_texcoords]);
		glBufferData(GL_ARRAY_BUFFER, m->vertex_count * 2 * sizeof(float), texcoords, GL_STATIC_DRAW);
	}

	glBindVertexArray(0);

	m->bounding = bounding_volume(count, positions);
	m->primitive = primitive;
}

void
mesh_index(struct mesh *m, size_t index_count, unsigned int *indices)
{

	if (!indices)
		return;

	m->idx_indices = m->vbo_count++;

	glBindVertexArray(m->vao);

	glGenBuffers(1, &m->vbo[m->idx_indices]);

	m->index_count = index_count;

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m->vbo[m->idx_indices]);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, m->index_count * sizeof(unsigned int), indices, GL_STATIC_DRAW);

	glBindVertexArray(0);
}

void
mesh_bind(struct mesh *m, GLint position, GLint normal, GLint texture)
{
	glBindVertexArray(m->vao);

	if (position >= 0) {
		glBindBuffer(GL_ARRAY_BUFFER, m->vbo[m->idx_positions]);
		glVertexAttribPointer(position, 3, GL_FLOAT, GL_FALSE, 0, 0);
		glEnableVertexAttribArray(position);
	}

	if (normal >= 0) {
		glBindBuffer(GL_ARRAY_BUFFER, m->vbo[m->idx_normals]);
		glVertexAttribPointer(normal, 3, GL_FLOAT, GL_FALSE, 0, 0);
		glEnableVertexAttribArray(normal);
	}

	if (texture >= 0) {
		glBindBuffer(GL_ARRAY_BUFFER, m->vbo[m->idx_texcoords]);
		glVertexAttribPointer(texture, 2, GL_FLOAT, GL_FALSE, 0, 0);
		glEnableVertexAttribArray(texture);
	}
}

void
mesh_free(struct mesh* m)
{
	if (m->vbo_count)
		glDeleteBuffers(m->vbo_count, m->vbo);
	m->vbo_count = 0;

	if (m->vao && glIsVertexArray(m->vao) == GL_TRUE)
		glDeleteVertexArrays(1, &m->vao);
}

void
mesh_load_quad(struct mesh *m, float x, float y)
{
	float positions[] = {
		-x, -y,  0,
		 x, -y,  0,
		-x,  y,  0,
		 x,  y,  0,
	};
	float texcoords[] = {
		0.0, 0.0,
		1.0, 0.0,
		0.0, 1.0,
		1.0, 1.0,
	};

	mesh_load(m, 4, GL_TRIANGLE_STRIP, positions, NULL, texcoords);
}

void
mesh_load_cross(struct mesh *m, float x)
{
	float positions[] = {
		-x,  0,  0,
		 x,  0,  0,
		 0, -x,  0,
		 0,  x,  0,
		 0,  0, -x,
		 0,  0,  x,
	};

	mesh_load(m, 6, GL_LINES, positions, NULL, NULL);
}

void
mesh_load_box(struct mesh *m, float a, float b, float c)
{
	/*
  a,b,-c +---+  -a,b,-c
        /   /|
a,b,c  +---+ -a,b,c
       |\, | |
       |  \| / -a,-b,-c
a,-b,c +---+'-a,-b,c
	 */
	float positions[] = {
		/* front */
		 a,  b,  c,   -a,  b,  c,   -a, -b,  c,   -a, -b,  c,    a, -b,  c,    a,  b,  c,
		/* back */
		-a,  b, -c,    a,  b, -c,    a, -b, -c,    a, -b, -c,   -a, -b, -c,   -a,  b, -c,
		/* left */
		 a,  b, -c,    a,  b,  c,    a, -b,  c,    a, -b,  c,    a, -b, -c,    a,  b, -c,
		/* right */
		-a,  b,  c,   -a,  b, -c,   -a, -b, -c,   -a, -b, -c,   -a, -b,  c,   -a,  b,  c,
		/* top */
		 a,  b, -c,   -a,  b, -c,   -a,  b,  c,   -a,  b,  c,    a,  b,  c,    a,  b, -c,
		/* bot */
		 a, -b,  c,   -a, -b,  c,   -a, -b, -c,   -a, -b, -c,    a, -b, -c,    a, -b,  c,
	};
	float normals[] = {
		 0,  0,  1,    0,  0,  1,    0,  0,  1,    0,  0,  1,    0,  0,  1,    0,  0,  1,
		 0,  0, -1,    0,  0, -1,    0,  0, -1,    0,  0, -1,    0,  0, -1,    0,  0, -1,
		 1,  0,  0,    1,  0,  0,    1,  0,  0,    1,  0,  0,    1,  0,  0,    1,  0,  0,
		-1,  0,  0,   -1,  0,  0,   -1,  0,  0,   -1,  0,  0,   -1,  0,  0,   -1,  0,  0,
		 0,  1,  0,    0,  1,  0,    0,  1,  0,    0,  1,  0,    0,  1,  0,    0,  1,  0,
		 0, -1,  0,    0, -1,  0,    0, -1,  0,    0, -1,  0,    0, -1,  0,    0, -1,  0,
	};
	float texcoords[] = {
		0, 1,    1, 1,    1, 0,    1, 0,    0, 0,    0, 1,
		0, 1,    1, 1,    1, 0,    1, 0,    0, 0,    0, 1,
		0, 1,    1, 1,    1, 0,    1, 0,    0, 0,    0, 1,
		0, 1,    1, 1,    1, 0,    1, 0,    0, 0,    0, 1,
		0, 1,    1, 1,    1, 0,    1, 0,    0, 0,    0, 1,
		0, 1,    1, 1,    1, 0,    1, 0,    0, 0,    0, 1,
	};
	mesh_load(m, 36, GL_TRIANGLES, positions, normals, texcoords);
}

void
mesh_load_cylinder(struct mesh *m, float h, float r, unsigned int sides)
{
	float positions[(sides + 1) * 3 * 2];
	float normals[(sides + 1) * 3 * 2];
	float texcoords[(sides + 1) * 2 * 2];
	float a, x, y, z;
	unsigned int s;

	for (s = 0; s < sides; s++) {
		a = 2 * M_PI * (s / (float)sides);
		x = r * sin(a);
		y = h * 0.5;
		z = r * cos(a);
		positions[s * 6 + 0] = x;
		positions[s * 6 + 1] = y;
		positions[s * 6 + 2] = z;
		x = r * sin(a);
		y = -h * 0.5;
		z = r * cos(a);
		positions[s * 6 + 3] = x;
		positions[s * 6 + 4] = y;
		positions[s * 6 + 5] = z;
	}

	/* require two vertices for different uv with same position */
	/* last vertex is at the same position as the first one */
	positions[sides * 6 + 0] = positions[0];
	positions[sides * 6 + 1] = positions[1];
	positions[sides * 6 + 2] = positions[2];
	positions[sides * 6 + 3] = positions[3];
	positions[sides * 6 + 4] = positions[4];
	positions[sides * 6 + 5] = positions[5];

	for (s = 0; s < sides; s++) {
		a = 2 * M_PI * (s / (float)sides);
		x = r * sin(a);
		z = r * cos(a);

		normals[s * 6 + 0] = x;
		normals[s * 6 + 1] = 0;
		normals[s * 6 + 2] = z;
		normals[s * 6 + 3] = x;
		normals[s * 6 + 4] = 0;
		normals[s * 6 + 5] = z;
	}
	normals[sides * 6 + 0] = normals[0];
	normals[sides * 6 + 1] = normals[1];
	normals[sides * 6 + 2] = normals[2];
	normals[sides * 6 + 3] = normals[3];
	normals[sides * 6 + 4] = normals[4];
	normals[sides * 6 + 5] = normals[5];

	for (s = 0; s < sides; s++) {
		a = s / (float)sides;
		texcoords[s * 4 + 0] = a;
		texcoords[s * 4 + 1] = 0;
		texcoords[s * 4 + 2] = a;
		texcoords[s * 4 + 3] = 1;
	}
	texcoords[sides * 4 + 0] = 1; /* sides / sides */
	texcoords[sides * 4 + 1] = texcoords[1];
	texcoords[sides * 4 + 2] = 1; /* sides / sides */
	texcoords[sides * 4 + 3] = texcoords[3];

	mesh_load(m, (sides + 1) * 2, GL_TRIANGLE_STRIP, positions, normals, texcoords);
}

void
mesh_load_torus(struct mesh *m, float circle_radius, float radial_radius,
		unsigned int circle_sides, unsigned int radial_sides)
{
	unsigned int vert_count = (circle_sides + 1) * (radial_sides + 1);
	float positions[vert_count * 3];
	float normals[vert_count * 3];
	float texcoords[vert_count * 2];
	unsigned int indices[radial_sides * (circle_sides + 1 + 2) * 2];
	float ra, ca, x, y, z;
	unsigned int r, c;
	vec3 cc, cp, cn;
	unsigned int off;

	for (r = 0; r < radial_sides; r++) {
		ra = 2 * M_PI * (r / (float) radial_sides);
		/* circle center */
		cc.x = radial_radius * cos(ra);
		cc.y = 0;
		cc.z = radial_radius * sin(ra);
		off = r * (circle_sides + 1);
		for (c = 0; c < circle_sides; c++) {
			ca = 2 * M_PI * (c / (float) circle_sides);
			x = cos(ra) * circle_radius * cos(ca);
			y = circle_radius * sin(ca);
			z = sin(ra) * circle_radius * cos(ca);

			positions[(off + c) * 3 + 0] = cc.x + x;
			positions[(off + c) * 3 + 1] = cc.y + y;
			positions[(off + c) * 3 + 2] = cc.z + z;

			cp.x = radial_radius * x;
			cp.y = radial_radius * y;
			cp.z = radial_radius * z;
			cn = vec3_normalize(cp);
			normals[(off + c) * 3 + 0] = cn.x;
			normals[(off + c) * 3 + 1] = cn.y;
			normals[(off + c) * 3 + 2] = cn.z;

			texcoords[(off + c) * 2 + 0] = (r / (float) radial_sides);
			texcoords[(off + c) * 2 + 1] = (c / (float) circle_sides);
		}
		{
			positions[(off + c) * 3 + 0] = positions[off * 3 + 0];
			positions[(off + c) * 3 + 1] = positions[off * 3 + 1];
			positions[(off + c) * 3 + 2] = positions[off * 3 + 2];
			normals[(off + c) * 3 + 0] = normals[off * 3 + 0];
			normals[(off + c) * 3 + 1] = normals[off * 3 + 1];
			normals[(off + c) * 3 + 2] = normals[off * 3 + 2];
			texcoords[(off + c) * 2 + 0] = (r / (float) radial_sides);
			texcoords[(off + c) * 2 + 1] = 1;
		}
	}
	{
		off = r * (circle_sides + 1);
		for (c = 0; c <= circle_sides; c++) {
			positions[(off + c) * 3 + 0] = positions[c * 3 + 0];
			positions[(off + c) * 3 + 1] = positions[c * 3 + 1];
			positions[(off + c) * 3 + 2] = positions[c * 3 + 2];
			normals[(off + c) * 3 + 0] = normals[c * 3 + 0];
			normals[(off + c) * 3 + 1] = normals[c * 3 + 1];
			normals[(off + c) * 3 + 2] = normals[c * 3 + 2];
			texcoords[(off + c) * 2 + 0] = 1;
			texcoords[(off + c) * 2 + 1] = (c / (float) circle_sides);
		}
	}

	for (r = 0; r < radial_sides; r++) {
		off = r * (circle_sides + 1 + 2);
		for (c = 0; c <= circle_sides; c++) {
			indices[(off + c) * 2 + 0] = (r + 1) * (circle_sides + 1) + c;
			indices[(off + c) * 2 + 1] = (r + 0) * (circle_sides + 1) + c;
		}
		{
			indices[(off + c) * 2 + 0] = (r + 1) * (circle_sides + 1) - 1;
			indices[(off + c) * 2 + 1] = (r + 1) * (circle_sides + 1) - 1;
			if (r + 2 <= radial_sides) {
				indices[(off + c) * 2 + 2] = (r + 2) * (circle_sides + 1);
				indices[(off + c) * 2 + 3] = (r + 2) * (circle_sides + 1);
			} else {
				indices[(off + c) * 2 + 2] = 0;
				indices[(off + c) * 2 + 3] = 0;
			}
		}
	}

	mesh_load(m, vert_count, GL_TRIANGLE_STRIP, positions, normals, texcoords);
	mesh_index(m, ARRAY_LEN(indices), indices);
}

void
mesh_load_bounding_sphere(struct mesh *m, float radius)
{
	int step = 24;
	float positions[3 * 3 * (step + 1)];
	float a;
	int off, i;

	/* Simple representation of a sphere as 3 circles.
	 *
	 * Generate 3 circles, each perpendicular to each other.
	 * In order to use GL_LINE_LOOP every line segment must
	 * be continuous, eg keep the pen on the paper.
	 *
	 * The trick is:
	 * let says each circle can be broken in 4 part or quarter.
	 * we have 3 circles a,b and c to draw, the total parts are:
	 *   a1 a2 a3 a4   b1 b2 b3 b4   c1 c2 c3 c4
	 *
	 * Now, one solution to draw the 3 circles in one stroke is
	 * the following:
	 *  - start with the first circle:       a1 a2 a3 a4
	 *  - do 1 quarter of the second circle: b1
	 *  - do the last circle:                c1 c2 c3 c4
	 *  - finish the second circle:             b2 b3 b4
	 *
	 * In memory we have the following sequence:
	 *   a1 a2 a3 a4   b1 c1 c2 c3   c4 b2 b3 b4
	 */

	for (i = 0; i < step; i++) {
		a = 2 * M_PI * (i / (float) step);

		off = 3 * (i + 0 * step);
		positions[off + 0] = radius * sin(a);
		positions[off + 1] = radius * cos(a);
		positions[off + 2] = 0;

		if (i < (step / 4))
			off = 3 * (i + 1 * step);
		else
			off = 3 * (i + 2 * step);
		positions[off + 0] = 0;
		positions[off + 1] = radius * cos(a);
		positions[off + 2] = radius * sin(a);

		if (i < (step / 4))
			off = 3 * (i + 2 * step);
		else
			off = 3 * (i + 1 * step);
		positions[off + 0] = radius * cos(a);
		positions[off + 1] = 0;
		positions[off + 2] = radius * sin(a);
	}

	mesh_load(m, 3 * step, GL_LINE_LOOP, positions, NULL, NULL);
}
