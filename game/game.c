#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>

#include <string.h>
#include "../engine/engine.h"
#include "game.h"
#include "asset.h"

#include <math.h>

static float
ray_distance_to_plane(vec3 org, vec3 dir, vec4 plane)
{
	/* p on ray   : p = t * dir + org */
	/* p on plane : (p-s) . n = 0 */
	/* t = (s-o).n / d.n */
	vec3 n = { plane.x , plane.y, plane.z };
	vec3 s = vec3_mult(plane.w, n);
	return vec3_dot(vec3_sub(s, org), n) / vec3_dot(dir, n);
}

static int
point_in_triangle(vec3 q, vec3 a, vec3 b, vec3 c)
{
	/* triangle:
	 *      + c
	 *     / \
	 *    /   \
	 * a +-----+ b
	 */
	/* n = (b - a) x (c - a) */
	vec3 n = vec3_normalize(vec3_cross(vec3_sub(b, a), vec3_sub(c, a)));
	vec3 e1 = vec3_sub(b, a);
	vec3 e2 = vec3_sub(c, b);
	vec3 e3 = vec3_sub(a, c);
	vec3 qa = vec3_sub(q, a);
	vec3 qb = vec3_sub(q, b);
	vec3 qc = vec3_sub(q, c);
	vec3 x1 = vec3_cross(e1, qa);
	vec3 x2 = vec3_cross(e2, qb);
	vec3 x3 = vec3_cross(e3, qc);

	return vec3_dot(x1, n) >= 0
		&& vec3_dot(x2, n) >= 0
		&& vec3_dot(x3, n) >= 0;
}

static vec4
ray_intersect_mesh(vec3 org, vec3 dir, struct mesh *mesh, mat4 *xfrm)
{
	vec4 q = { 0 };
	unsigned int i;
	float dist = 10000.0; /* TODO: find a sane max value */
	float *pos = mesh->positions;

	if (!pos)
		return q;
	if (mesh->primitive != GL_TRIANGLES)
		return q; /* not triangulated, need indexes as well */

	for (i = 0; i < mesh->vertex_count; i += 3) {
		size_t idx = i * 3;
		vec3 t1 = mat4_mult_vec3(xfrm, (vec3){ pos[idx + 0], pos[idx + 1], pos[idx + 2] });
		vec3 t2 = mat4_mult_vec3(xfrm, (vec3){ pos[idx + 3], pos[idx + 4], pos[idx + 5] });
		vec3 t3 = mat4_mult_vec3(xfrm, (vec3){ pos[idx + 6], pos[idx + 7], pos[idx + 8] });
		vec3 n = vec3_normalize(vec3_cross(vec3_sub(t2, t1), vec3_sub(t3, t1)));
		vec4 plane = { n.x, n.y, n.z, vec3_dot(t1, n)};
		float d = ray_distance_to_plane(org, dir, plane);
		if (d >= 0 && d < dist) {
			vec3 p = vec3_add(vec3_mult(d, dir), org);
			if (point_in_triangle(p, t1, t2, t3)) {
				dist = d;
				q = (vec4) { p.x, p.y, p.z, d };
			}
		}
	}
	return q;
}

void *
mempush(struct memory_zone *zone, size_t size)
{
	void *addr = NULL;

	if (zone->used + size <= zone->size) {
		addr = zone->base + zone->used;
		zone->used += size;
	} else {
		die("mempush: Not enough memory\n");
	}

	return addr;
}

void
mempull(struct memory_zone *zone, size_t size)
{
	if (size > zone->used)
		die("mempull: Freed too much memory\n");

	zone->used -= size;
}

struct texture {
	GLuint id;
	GLenum type;
	size_t width, height;
};

struct game_state {
	struct game_asset *game_asset;
	struct game_input input;
	struct window_io *window_io;

	enum {
		GAME_MENU,
		GAME_PLAY,
		GAME_PAUSE,
	} state, new_state;

#define MENU_SEL_NONE 0
#define MENU_SEL_PLAY 1
#define MENU_SEL_QUIT 2
	int menu_selection;

	struct camera cam;
	int flycam;
	int flycam_forward, flycam_left;
	float flycam_speed;

	quaternion player_dir;
	float player_lookup;
	vec3 player_pos;
	vec3 player_new_pos;
	float player_speed;

	struct sampler theme_sampler;
	struct wav *theme_wav;

	struct sampler wind_sampler;
	struct wav *wind_wav;
	int debug;
};

static float clamp(float v, float a, float b)
{
	if (v < a)
		return a;
	if (v > b)
		return b;
	return v;
}

static struct texture
create_2d_tex(size_t w, size_t h, void *data)
{
	struct texture tex;

	tex.type = GL_TEXTURE_2D;
	tex.width = w;
	tex.height = h;

	glGenTextures(1, &tex.id);
	glBindTexture(tex.type, tex.id);

	glTexParameteri(tex.type, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(tex.type, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(tex.type, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(tex.type, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	glTexImage2D(tex.type, 0, GL_RED, w, h, 0, GL_RED, GL_UNSIGNED_BYTE, data);

	return tex;
}

void
game_init(struct game_memory *game_memory, struct file_io *file_io, struct window_io *win_io)
{
	struct game_state *game_state;
	struct game_asset *game_asset;

	game_state = mempush(&game_memory->state, sizeof(struct game_state));
	game_asset = mempush(&game_memory->asset, sizeof(struct game_asset));

	game_state->game_asset = game_asset;
	game_asset_init(game_asset, &game_memory->asset, &game_memory->audio, file_io);

	camera_init(&game_state->cam, 1.05, 1);
	camera_set(&game_state->cam, (vec3){0, 1, -5}, QUATERNION_IDENTITY);

	game_state->window_io = win_io;
	game_state->state = GAME_MENU;

	/* audio */

	game_state->theme_wav = game_get_wav(game_asset, WAV_THEME);
	sampler_init(&game_state->theme_sampler, game_state->theme_wav);
	game_state->theme_sampler.loop_on = 1;
	game_state->theme_sampler.trig_on = 1;

	game_state->wind_wav = game_get_wav(game_asset, WAV_WIND);
	sampler_init(&game_state->wind_sampler, game_state->wind_wav);
	game_state->wind_sampler.loop_on = 1;
	game_state->wind_sampler.trig_on = 1;
	game_state->wind_sampler.loop_start = 805661; /* Loop start after fadein */
}

void
game_fini(struct game_memory *memory)
{
	struct game_asset *game_asset = memory->asset.base;
	game_asset_fini(game_asset);
}

static void
game_input(struct game_state *game_state, struct game_input *input)
{
	unsigned int key;
	unsigned int action;
	double dx, dy;

	if (game_state->input.width != input->width ||
	    game_state->input.height != input->height) {
		glViewport(0, 0, input->width, input->height);
		camera_set_ratio(&game_state->cam, (float)input->width / (float)input->height);
		game_state->input.width = input->width;
		game_state->input.height = input->height;
	}

	dx = input->xinc;
	dy = input->yinc;
	game_state->input.xpos = input->xpos;
	game_state->input.ypos = input->ypos;
	game_state->input.xinc = input->xinc;
	game_state->input.yinc = input->yinc;

	for (key = 0; key < ARRAY_LEN(input->keys); key++) {
		if (game_state->input.keys[key] == input->keys[key])
			continue;

		action = input->keys[key];
		switch (key) {
		case KEY_ESCAPE:
			/* glfwSetWindowShouldClose(window, TRUE); */
			game_state->new_state = GAME_MENU;
			break;
		case 'F':
			if (game_state->debug && action == KEY_PRESSED)
				game_asset_fini(game_state->game_asset);
			break;
		case KEY_LEFT_SHIFT:
		case KEY_RIGHT_SHIFT:
			game_state->flycam_speed = (action == KEY_PRESSED) ? 10 : 1;
			break;
		case 'Z':
			if (action == KEY_PRESSED) {
				game_state->flycam_speed = 1;
				game_state->flycam = !game_state->flycam;
			}
			break;
		case 'X':
			if (action == KEY_PRESSED)
				game_state->debug = !game_state->debug;
			break;
		case 'A':
		case KEY_LEFT:
			if (action == KEY_PRESSED)
				game_state->flycam_left = 1;
			else if (game_state->flycam_left > 0)
				game_state->flycam_left = 0;
			break;
		case 'D':
		case KEY_RIGHT:
			if (action == KEY_PRESSED)
				game_state->flycam_left = -1;
			else if (game_state->flycam_left < 0)
				game_state->flycam_left = 0;
			break;
		case 'W':
		case KEY_UP:
			if (action == KEY_PRESSED)
				game_state->flycam_forward = 1;
			else if (game_state->flycam_forward > 0)
				game_state->flycam_forward = 0;
			break;
		case 'S':
		case KEY_DOWN:
			if (action == KEY_PRESSED)
				game_state->flycam_forward = -1;
			else if (game_state->flycam_forward < 0)
				game_state->flycam_forward = 0;
			break;
		default:
			break;
		}
		game_state->input.keys[key] = input->keys[key];
	}

	game_state->input.buttons[0] = input->buttons[0];

	/* This is the debug flycam */
	if (game_state->flycam) {
		vec3 left;

		camera_rotate(&game_state->cam, VEC3_AXIS_Y, -0.001 * dx);
		left = camera_get_left(&game_state->cam);
		left = vec3_normalize(left);
		camera_rotate(&game_state->cam, left, 0.001 * dy);
	} else {
		quaternion q = quaternion_axis_angle(VEC3_AXIS_Y, -0.001 * dx);
		game_state->player_dir = quaternion_mult(q, game_state->player_dir);
		game_state->player_lookup += 0.001 * dy;
		game_state->player_lookup = clamp(game_state->player_lookup, -0.5 * M_PI, 0.5 * M_PI);
	}
}

enum entity_type {
	ENTITY_DEBUG,
	ENTITY_UI,
	ENTITY_COUNT
};

struct entity {
	enum entity_type type;
	enum asset_key shader;
	enum asset_key mesh;
	int mode; /* GL_LINE of GL_FILL */
	quaternion rotation;
	vec3 position;
	vec3 scale;
	vec3 color;
	struct entity_textures {
		const char *name;
		struct texture *texture;
	} textures[8];
};

struct render_queue {
	struct memory_zone zone;
	size_t count;
	struct game_state *game_state;
	struct game_asset *game_asset;
};

static void
render_queue_init(struct render_queue *queue,
		  struct game_state *game_state,
		  struct game_asset *game_asset,
		  void *base, size_t size)
{
	queue->zone.base = base;
	queue->zone.size = size;
	queue->zone.used = 0;
	queue->count = 0;
	queue->game_state = game_state;
	queue->game_asset = game_asset;
}

static void
render_queue_push(struct render_queue *queue, struct entity *entity)
{
	struct entity *entry;

	entry = mempush(&queue->zone, sizeof(*entity));
	*entry = *entity;
	queue->count++;
}

static void
render_bind_shader(struct shader *shader)
{
	/* Set the current shader program to shader->prog */
	glUseProgram(shader->prog);
}

static void
render_bind_mesh(struct shader *shader, struct mesh *mesh)
{
	GLint position;
	GLint normal;
	GLint texcoord;

	position = glGetAttribLocation(shader->prog, "in_pos");
	normal = glGetAttribLocation(shader->prog, "in_normal");
	texcoord = glGetAttribLocation(shader->prog, "in_texcoord");

	mesh_bind(mesh, position, normal, texcoord);
}

static void
render_bind_camera(struct shader *s, struct camera *c)
{
	GLint proj, view;

	proj = glGetUniformLocation(s->prog, "proj");
	view = glGetUniformLocation(s->prog, "view");

	glUniformMatrix4fv(proj, 1, GL_FALSE, (float *)&c->proj.m);
	glUniformMatrix4fv(view, 1, GL_FALSE, (float *)&c->view.m);
}

static int
frustum_cull(vec4 frustum[6], struct mesh *mesh, vec3 pos, vec3 scale)
{
	vec3 center = mesh->bounding.off;
	float radius = vec3_max(scale) * mesh->bounding.radius;

	/* move the sphere center to the object position */
	pos = vec3_fma(scale, center, pos);

	return sphere_outside_frustum(frustum, pos, 0.5 * radius);
}

static void
render_mesh(struct mesh *mesh)
{
	if (mesh->index_count > 0)
		glDrawElements(mesh->primitive, mesh->index_count, GL_UNSIGNED_INT, 0);
	else
		glDrawArrays(mesh->primitive, 0, mesh->vertex_count);
}

static void
render_queue_exec(struct render_queue *queue)
{
	struct game_state *game_state = queue->game_state;
	struct game_asset *game_asset = queue->game_asset;
	struct entity *entry = queue->zone.base;
	struct camera *cam = &game_state->cam;
	int last_mode = 0;
	enum asset_key last_shader = ASSET_KEY_COUNT;
	enum asset_key last_mesh = ASSET_KEY_COUNT;
	struct shader *shader = NULL;
	struct mesh *mesh = NULL;
	unsigned int i;

	glDepthMask(GL_TRUE);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);

	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	for (i = 0; i < queue->count; i++) {
		struct entity e = entry[i];

		if (!shader || last_shader != e.shader) {
			last_shader = e.shader;
			shader = game_get_shader(game_asset, e.shader);
			render_bind_shader(shader);
			render_bind_camera(shader, cam);
			mesh = NULL; /* mesh need to be bind again */
		}
		if (!mesh || last_mesh != e.mesh) {
			last_mesh = e.mesh;
			mesh = game_get_mesh(game_asset, e.mesh);
			render_bind_mesh(shader, mesh);
		}
		mat4 transform = mat4_transform_scale(e.position,
						      e.rotation,
						      e.scale);

		GLint model = glGetUniformLocation(shader->prog, "model");
		if (model >= 0)
			glUniformMatrix4fv(model, 1, GL_FALSE, (float *)&transform.m);

		GLint time = glGetUniformLocation(shader->prog, "time");
		if (time >= 0)
			glUniform1f(time, game_state->input.time);

		GLint camp = glGetUniformLocation(shader->prog, "camp");
		if (camp >= 0)
			glUniform3f(camp, cam->position.x, cam->position.y, cam->position.z);

		GLint color = glGetUniformLocation(shader->prog, "color");
		if (color >= 0)
			glUniform3f(color, e.color.x, e.color.y, e.color.z);

		GLint v2res = glGetUniformLocation(shader->prog, "v2Resolution");
		if (v2res >= 0)
			glUniform2f(v2res, game_state->input.width, game_state->input.height);

		if (last_mode != e.mode) {
			last_mode = e.mode;
			switch (e.mode) {
			case GL_LINE:
			case GL_FILL:
				glPolygonMode(GL_FRONT_AND_BACK, e.mode);
				break;
			default:
				glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
				break;
			}
		}
		switch (e.type) {
		default:
			render_mesh(mesh);
			break;
		}
	}
}

struct scene {
	unsigned int count;
	struct entity *entity;
};

static void
render_scene(struct game_state *game_state,
	     struct game_asset *game_asset,
	     struct scene *scene,
	     struct render_queue *rqueue)
{
	unsigned int i;
	struct camera *cam = &game_state->cam;
	mat4 vm = mat4_mult_mat4(&cam->proj, &cam->view);
	vec4 frustum[6];

	mat4_projection_frustum(&vm, frustum);

	for (i = 0; i < scene->count; i++) {
		struct entity *e = &scene->entity[i];
		struct mesh *mesh = game_get_mesh(game_asset, e->mesh);

		if (frustum_cull(frustum, mesh, e->position, e->scale))
			continue;

		render_queue_push(rqueue, e);
	}
}

static void
debug_origin_mark(struct render_queue *rqueue)
{
	render_queue_push(rqueue, &(struct entity){
			.type = ENTITY_DEBUG,
			.shader = SHADER_SOLID,
			.mesh = DEBUG_MESH_CROSS,
			.scale = {1, 0, 0},
			.position = {0, 0, 0},
			.rotation = QUATERNION_IDENTITY,
			.color = {1, 0, 0},
			.mode = GL_LINE,
		});
	render_queue_push(rqueue, &(struct entity){
			.type = ENTITY_DEBUG,
			.shader = SHADER_SOLID,
			.mesh = DEBUG_MESH_CROSS,
			.scale = {0, 1, 0},
			.position = {0, 0, 0},
			.rotation = QUATERNION_IDENTITY,
			.color = {0, 1, 0},
			.mode = GL_LINE,
		});
	render_queue_push(rqueue, &(struct entity){
			.type = ENTITY_DEBUG,
			.shader = SHADER_SOLID,
			.mesh = DEBUG_MESH_CROSS,
			.scale = {0, 0, 1},
			.position = {0, 0, 0},
			.rotation = QUATERNION_IDENTITY,
			.color = {0, 0, 1},
			.mode = GL_LINE,
		});
}

static void
flycam_move(struct game_state *game_state, float dt)
{
	vec3 forw = camera_get_dir(&game_state->cam);
	vec3 left = camera_get_left(&game_state->cam);
	vec3 dir;
	float speed = game_state->flycam_speed * dt;

	if (!(game_state->flycam_left || game_state->flycam_forward))
		return;

	forw = vec3_mult(game_state->flycam_forward, forw);
	left = vec3_mult(game_state->flycam_left, left);
	dir = vec3_add(forw, left);
	dir = vec3_normalize(dir);
	dir = vec3_mult(speed, dir);

	camera_move(&game_state->cam, dir);
}


static void
game_enter_state(struct game_state *game_state, int state)
{
	switch (state) {
	case GAME_MENU:
		game_state->window_io->cursor(1); /* show */
		break;
	case GAME_PLAY:
		game_state->window_io->cursor(0); /* hide */
		break;
	default:
		break;
	}
	game_state->state = state;
}

static void
game_menu(struct game_state *game_state, struct render_queue *rqueue)
{
	struct game_input *input = &game_state->input;
	float ratio = (double)input->width / (double)input->height;
	vec3 scale = { 0.25, 0.25 * ratio, 0};
	vec3 color_default  = {0.7,0.7,0.7};
	vec3 color_selected = {0.9,0.9,0.9};
	vec3 cursor = { 0 };
	int sel = game_state->menu_selection;
	cursor.x = input->xpos / (double) input->width;
	cursor.y = input->ypos / (double) input->height;
	cursor.x = cursor.x * 2.0 - 1.0;
	cursor.y = cursor.y * 2.0 - 1.0;
	cursor.y *= -1;

	if (input->xinc || input->yinc) {
		if (0.125 < cursor.y && cursor.y < 0.25)
			sel = MENU_SEL_PLAY;
		if (0 > cursor.y && cursor.y > -0.125)
			sel = MENU_SEL_QUIT;
	}

	render_queue_push(rqueue, &(struct entity){
			.type = ENTITY_UI,
			.shader = SHADER_TEXT,
			.mesh = MESH_MENU_START,
			.scale = scale,
			.position = {0, 0.125, 0},
			.rotation = QUATERNION_IDENTITY,
			.color = (sel == MENU_SEL_PLAY) ? color_selected : color_default,
		});
	render_queue_push(rqueue, &(struct entity){
			.type = ENTITY_UI,
			.shader = SHADER_TEXT,
			.mesh = MESH_MENU_QUIT,
			.scale = scale,
			.position = {0, -0.125, 0},
			.rotation = QUATERNION_IDENTITY,
			.color = (sel == MENU_SEL_QUIT) ? color_selected : color_default,
		});
	if (input->keys[KEY_UP] == KEY_PRESSED) {
		sel = MENU_SEL_PLAY;
	} else if (input->keys[KEY_DOWN] == KEY_PRESSED) {
		sel = MENU_SEL_QUIT;
	}

	if (input->keys[KEY_ENTER] == KEY_PRESSED) {
		switch (sel) {
		case MENU_SEL_PLAY:
			game_state->new_state = GAME_PLAY;
			break;
		case MENU_SEL_QUIT:
			game_state->window_io->close();
			break;
		}
	}
	game_state->menu_selection = sel;
}

struct entity level_1[] = {
	{ .type = 0, .mesh = MESH_WALL, .shader = SHADER_WALL, .position = { 0, 0, 0}, .scale = {1,1,1}, .color = {0,1,0}},
	{ .type = 0, .mesh = MESH_WALL, .shader = SHADER_WALL, .position = { 0, 2, 0}, .scale = {1,1,1}, .color = {0,0,1}},
};

static void
game_play(struct game_state *game_state, struct game_asset *game_asset, struct render_queue *rqueue)
{
	struct scene scene = {
		.count = ARRAY_LEN(level_1),
		.entity = level_1,
	};

	render_scene(game_state, game_asset, &scene, rqueue);
}

void
game_step(struct game_memory *memory, struct game_input *input, struct game_audio *audio)
{
	struct game_state *game_state = memory->state.base;
	struct game_asset *game_asset = memory->asset.base;
	struct render_queue rqueue;
	float dt = input->time - game_state->input.time;

	memory->scrap.used = 0;
	render_queue_init(&rqueue, game_state, game_asset,
			  mempush(&memory->scrap, SZ_4M), SZ_4M);

	if (game_state->state != game_state->new_state)
		game_enter_state(game_state, game_state->new_state);

	game_input(game_state, input);
	switch (game_state->state) {
	case GAME_MENU:
		game_menu(game_state, &rqueue);
		break;
	case GAME_PLAY:
		game_play(game_state, game_asset, &rqueue);
		break;
	default:
		break;
	}
	if (game_state->debug)
		debug_origin_mark(&rqueue);
	if (game_state->flycam)
		flycam_move(game_state, dt);

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	render_queue_exec(&rqueue);

	/* audio */
	float sample_l, sample_r;
	for (int i = 0; i < audio->size; i++) {
		if (game_state->state == PLAY) {
			sample_l = step_sampler(&game_state->wind_sampler);
			sample_r = step_sampler(&game_state->wind_sampler);
		}
		else {
			sample_l = step_sampler(&game_state->theme_sampler);
			sample_r = step_sampler(&game_state->theme_sampler);
		}
			audio->buffer[i].r = sample_r;
			audio->buffer[i].l = sample_l;
	}

	game_asset_poll(game_asset);
}
