#ifndef MESH_H
#define MESH_H

#define MESH_ATTRIB_POSITION 0
#define MESH_ATTRIB_NORMAL   1
#define MESH_ATTRIB_TEXCOORD 2
#define MESH_MAX_VBO 4

struct mesh {
	GLuint vao;
	GLuint vbo[MESH_MAX_VBO];
	int vbo_count;
	int idx_positions;
	int idx_normals;
	int idx_texcoords;
	int idx_indices;
	size_t vertex_count;
	size_t index_count;
	GLenum primitive;
	struct bounding_volume {
		vec3 min;
		vec3 max;
		vec3 ext;
		vec3 off; /* offset to the mesh origin */
		float radius; /* bounding sphere radius */
	} bounding;
	float *positions;
};

/** mesh_load
   specifications:
   void mesh_load(struct mesh *m, size_t count, GLenum primitive, float *positions,
   float *normals, float *texcoords)

   Semantics: We want to pass a 3D model to openGL for rendering.
   mesh load(m, count, position, normals, texcoords) takes as input:
    - count, the number of vertices in the model.
    - positions, a float array representing the vertices coordinates of the model.

   And (optionals) float arrays representing vertices attributes:
    - normals, nomals coordinates
    - texcoord, texture coordinates.

   The function initialize GL vertex buffer(s) object and load them with the model
   data.
   It also initialize the values in the mesh_struct record m passed as a parameter.
   Preconditions:
    - m must be a valid pointer to a initialized stuct mesh.
    - count, positions, normal must represent a 3d object with triangulated faces.
      we assume the values in those data structures are correct.
   Rem:
     coun, positions, normals can be initialized from a wavefront object
     using load_obj. (see asset.h).
*/
void mesh_load(struct mesh *m, size_t count, GLenum primitive, float *positions, float *normals, float *texcoords);
void mesh_index(struct mesh *m, size_t count, unsigned int *index);
void mesh_bind(struct mesh *m, GLint position, GLint normal, GLint texture);
void mesh_free(struct mesh *m);
void mesh_load_box(struct mesh *m, float x, float y, float z);
void mesh_load_quad(struct mesh *m, float x, float y);
void mesh_load_cross(struct mesh *m, float x);
void mesh_load_torus(struct mesh *m, float circle_radius, float radial_radius, unsigned int circle_sides, unsigned int radial_sides);
void mesh_load_cylinder(struct mesh *m, float h, float r, unsigned int sides);
void mesh_load_bounding_sphere(struct mesh *m, float r);
#endif
