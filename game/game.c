#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>

#include <string.h>
#include "game.h"
#include "asset.h"

#include <math.h>

#define glPolygonMode(a,b)
#define GL_LINE 0
#define GL_FILL 1

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

struct texture {
	GLuint id;
	GLenum type;
	size_t width, height;
};

struct game_state {
	struct game_asset *game_asset;
	struct input input;
	struct window_io *window_io;
	float last_time;

	enum {
		GAME_INIT,
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
	int key_flycam;

	quaternion player_dir;
	float player_lookup;
	vec3 player_pos;
	vec3 player_new_pos;
	float player_speed;
	vec3 player_aim;

	struct sampler theme_sampler;
	struct wav *theme_wav;

	struct sampler casey_sampler;
	struct wav *casey_wav;

	struct sampler wind_sampler;
	struct wav *wind_wav;

	struct sampler menu_sampler;
	struct wav *menu_wav;

	struct sampler woosh_sampler[4];
	struct wav *woosh_wav[4];

	struct sampler crash_sampler[4];
	struct wav *crash_wav[4];

	int debug;
	int key_debug;

	int round;
	struct rock {
		vec3 pos;
		quaternion dir;
		short vld;
		short trg;
	} rocks[20];
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
	game_state->flycam_speed = 1;

	game_state->window_io = win_io;
	game_state->state = GAME_INIT;
	game_state->new_state = GAME_MENU;

	/* audio */
	game_state->theme_wav = game_get_wav(game_asset, WAV_THEME);
	sampler_init(&game_state->theme_sampler, game_state->theme_wav);
	game_state->theme_sampler.loop_on = 1;
	game_state->theme_sampler.trig_on = 1;

	game_state->casey_wav = game_get_wav(game_asset, WAV_CASEY);
	sampler_init(&game_state->casey_sampler, game_state->casey_wav);
	game_state->casey_sampler.loop_on = 1;
	game_state->casey_sampler.trig_on = 1;
	game_state->casey_sampler.vol = 0.8;
	game_state->casey_sampler.loop_start = 7899500 * 2;

	game_state->wind_wav = game_get_wav(game_asset, WAV_WIND);
	sampler_init(&game_state->wind_sampler, game_state->wind_wav);
	game_state->wind_sampler.loop_on = 1;
	game_state->wind_sampler.trig_on = 1;
	game_state->wind_sampler.loop_start = 805661 * 2; /* Loop start after fadein */

	game_state->menu_wav = game_get_wav(game_asset, WAV_MENU);
	sampler_init(&game_state->menu_sampler, game_state->menu_wav);
	game_state->menu_sampler.vol = 0.3;

	for (size_t i = 0; i < 4; i++) {
		game_state->woosh_wav[i] =
			game_get_wav(game_asset, WAV_WOOSH_00 + i);
		sampler_init(&game_state->woosh_sampler[i],
			     game_state->woosh_wav[i]);
		game_state->woosh_sampler[i].vol = 0.4;
	}
	for (size_t i = 0; i < 4; i++) {
		game_state->crash_wav[i] =
			game_get_wav(game_asset, WAV_CRASH_00 + i);
		sampler_init(&game_state->crash_sampler[i],
			     game_state->crash_wav[i]);
		game_state->crash_sampler[i].vol = 0.4;
	}
}

void
game_fini(struct game_memory *memory)
{
	struct game_asset *game_asset = memory->asset.base;
	game_asset_fini(game_asset);
}

enum entity_type {
	ENTITY_GAME,
	ENTITY_SCREEN,
	ENTITY_UI,
	ENTITY_DEBUG,
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
		if (!game_state->debug && e.type == ENTITY_DEBUG)
			continue;

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
			glUniform1f(time, game_state->last_time);

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
flycam_move(struct game_state *game_state, struct input *input, float dt)
{
	vec3 forw = camera_get_dir(&game_state->cam);
	vec3 left = camera_get_left(&game_state->cam);
	vec3 dir = { 0 };
	float speed = game_state->flycam_speed * dt;

	game_state->flycam_forward = 0;
	if (key_pressed(input, 'W'))
		game_state->flycam_forward = 1;
	else if (key_pressed(input, 'S'))
		game_state->flycam_forward = -1;
	game_state->flycam_left = 0;
	if (key_pressed(input, 'A'))
		game_state->flycam_left = 1;
	else if (key_pressed(input, 'D'))
		game_state->flycam_left = -1;

	if (game_state->flycam_left || game_state->flycam_forward) {
		forw = vec3_mult(game_state->flycam_forward, forw);
		left = vec3_mult(game_state->flycam_left, left);
		dir = vec3_add(forw, left);
		dir = vec3_normalize(dir);
		dir = vec3_mult(speed, dir);
		camera_move(&game_state->cam, dir);
	}

	float dx = input->xinc;
	float dy = input->yinc;

	if (dx || dy) {
		camera_rotate(&game_state->cam, VEC3_AXIS_Y, -0.001 * dx);
		left = camera_get_left(&game_state->cam);
		left = vec3_normalize(left);
		camera_rotate(&game_state->cam, left, 0.001 * dy);
	}

	/* drop camera config to stdout */
	if (key_pressed(input, KEY_SPACE)) {
		printf("camera position:\n");
		print_vec3(game_state->cam.position);
		printf("camera rotation:\n");
		print_vec3(game_state->cam.rotation.v);
		printf("%f\n", game_state->cam.rotation.w);
	}
}


static void
game_enter_state(struct game_state *game_state, int state)
{
	switch (state) {
	case GAME_MENU:
	{
		vec3 pos = {    0.38,     2.59,    -1.51};
		quaternion rot = {{   -0.00,    -0.13,     0.00}, 0.991398};
		camera_set(&game_state->cam, pos, rot);
		for (size_t i = 0; i < 4; i++) {
		sampler_init(&game_state->woosh_sampler[i],
			     game_state->woosh_wav[i]);
		}
		for (size_t i = 0; i < 4; i++) {
			sampler_init(&game_state->crash_sampler[i],
				     game_state->crash_wav[i]);
		}
	}
		game_state->window_io->cursor(1); /* show */
		break;
	case GAME_PLAY:
		game_state->player_speed = 1;
		game_state->player_aim = VEC3_ZERO;
		game_state->player_pos = VEC3_ZERO;
		game_state->player_dir = QUATERNION_IDENTITY;
		game_state->round = 0;
		for (int i = 0; i < 20; i++) {
			game_state->rocks[i].vld = 0;
			game_state->rocks[i].pos = VEC3_ZERO;
		}
		game_state->window_io->cursor(0); /* hide */
		break;
	default:
		break;
	}
	game_state->state = state;
}

static void
game_menu(struct game_state *game_state, struct input *input, struct render_queue *rqueue)
{
	float ratio = (double)input->width / (double)input->height;
	vec3 scale = { 0.25, 0.25 * ratio, 0};
	vec3 color_default  = {0.7,0.7,0.7};
	vec3 color_selected = {0.9,0.9,0.9};
	vec3 cursor = { 0 };
	int sel = game_state->menu_selection;
	float time = input->time;
	cursor.x = input->xpos / (double) input->width;
	cursor.y = input->ypos / (double) input->height;
	cursor.x = cursor.x * 2.0 - 1.0;
	cursor.y = cursor.y * 2.0 - 1.0;
	cursor.y *= -1;

	if (input->xinc || input->yinc) {
		if (0.125 < cursor.y && cursor.y < 0.25) {
			sel = MENU_SEL_PLAY;
		}
		if (0 > cursor.y && cursor.y > -0.125) {
			sel = MENU_SEL_QUIT;
		}
	}

	render_queue_push(rqueue, &(struct entity){
			.type = ENTITY_GAME,
			.shader = time > (15.0*60.0) ? SHADER_SCREEN : SHADER_WALL,
			.mesh = MESH_ROOM,
			.scale = {1,1,1},
			.position = {0, 0, 0},
			.rotation = QUATERNION_IDENTITY,
		});
	render_queue_push(rqueue, &(struct entity){
			.type = ENTITY_SCREEN,
			.shader = SHADER_SCREEN,
			.mesh = MESH_SCREEN,
			.scale = {1,1,1},
			.position = {0, 0, 0},
			.rotation = QUATERNION_IDENTITY,
		});
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
	if (key_pressed(input, KEY_UP)) {
		sel = MENU_SEL_PLAY;
	} else if (key_pressed(input, KEY_DOWN)) {
		sel = MENU_SEL_QUIT;
	}

	if (key_pressed(input, KEY_ENTER) || input->buttons[0]) {
		switch (sel) {
		case MENU_SEL_PLAY:
			game_state->new_state = GAME_PLAY;
			break;
		case MENU_SEL_QUIT:
			game_state->window_io->close();
			break;
		}
	}
	if (game_state->menu_selection != sel) {
		game_state->menu_sampler.trig_on = 1;
#if 0
		size_t i = rand()%4;
		game_state->crash_sampler[i].trig_on = 1;
#endif
	}

	game_state->menu_selection = sel;

}

static void
game_play(struct game_state *game_state, struct input *input, float dt, struct render_queue *rqueue)
{
	struct game_asset *game_asset = game_state->game_asset;
	vec3 wall_scale = (vec3){1, 1, 1};
	float wallext = wall_scale.y * 40;
	struct entity level_1[] = {
		{ .type = 0, .mesh = MESH_WALL, .shader = SHADER_WALL, .scale = wall_scale, .color = {},
		  .position = { 0, wallext * (0 - 10), 0}, .rotation = quaternion_axis_angle(VEC3_AXIS_Y, 0.1) },
		{ .type = 0, .mesh = MESH_WALL, .shader = SHADER_WALL, .scale = wall_scale, .color = {},
		  .position = { 0, wallext * (1 - 10), 0}, .rotation = quaternion_axis_angle(VEC3_AXIS_Y, 0.4) },
		{ .type = 0, .mesh = MESH_WALL, .shader = SHADER_WALL, .scale = wall_scale, .color = {},
		  .position = { 0, wallext * (2 - 10), 0}, .rotation = quaternion_axis_angle(VEC3_AXIS_Y, 2.4) },
		{ .type = 0, .mesh = MESH_WALL, .shader = SHADER_WALL, .scale = wall_scale, .color = {},
		  .position = { 0, wallext * (3 - 10), 0}, .rotation = quaternion_axis_angle(VEC3_AXIS_Y, 0.3) },
		{ .type = 0, .mesh = MESH_WALL, .shader = SHADER_WALL, .scale = wall_scale, .color = {},
		  .position = { 0, wallext * (4 - 10), 0}, .rotation = quaternion_axis_angle(VEC3_AXIS_Y, 1.7) },
		{ .type = 0, .mesh = MESH_WALL, .shader = SHADER_WALL, .scale = wall_scale, .color = {},
		  .position = { 0, wallext * (5 - 10), 0}, .rotation = quaternion_axis_angle(VEC3_AXIS_Y, 1.1) },
		{ .type = 0, .mesh = MESH_WALL, .shader = SHADER_WALL, .scale = wall_scale, .color = {},
		  .position = { 0, wallext * (6 - 10), 0}, .rotation = quaternion_axis_angle(VEC3_AXIS_Y, 2.1) },
		{ .type = 0, .mesh = MESH_WALL, .shader = SHADER_WALL, .scale = wall_scale, .color = {},
		  .position = { 0, wallext * (7 - 10), 0}, .rotation = quaternion_axis_angle(VEC3_AXIS_Y, 1.2) },
		{ .type = 0, .mesh = MESH_WALL, .shader = SHADER_WALL, .scale = wall_scale, .color = {},
		  .position = { 0, wallext * (8 - 10), 0}, .rotation = quaternion_axis_angle(VEC3_AXIS_Y, 3.4) },
		{ .type = 0, .mesh = MESH_WALL, .shader = SHADER_WALL, .scale = wall_scale, .color = {},
		  .position = { 0, wallext * (9 - 10), 0}, .rotation = quaternion_axis_angle(VEC3_AXIS_Y, 0.1) },

		{ .type = 0, .mesh = MESH_WALL, .shader = SHADER_WALL, .scale = wall_scale, .color = {},
		  .position = { 0, wallext * 0, 0}, .rotation = quaternion_axis_angle(VEC3_AXIS_Y, 0.1) },
		{ .type = 0, .mesh = MESH_WALL, .shader = SHADER_WALL, .scale = wall_scale, .color = {},
		  .position = { 0, wallext * 1, 0}, .rotation = quaternion_axis_angle(VEC3_AXIS_Y, 0.4) },
		{ .type = 0, .mesh = MESH_WALL, .shader = SHADER_WALL, .scale = wall_scale, .color = {},
		  .position = { 0, wallext * 2, 0}, .rotation = quaternion_axis_angle(VEC3_AXIS_Y, 2.4) },
		{ .type = 0, .mesh = MESH_WALL, .shader = SHADER_WALL, .scale = wall_scale, .color = {},
		  .position = { 0, wallext * 3, 0}, .rotation = quaternion_axis_angle(VEC3_AXIS_Y, 0.3) },
		{ .type = 0, .mesh = MESH_WALL, .shader = SHADER_WALL, .scale = wall_scale, .color = {},
		  .position = { 0, wallext * 4, 0}, .rotation = quaternion_axis_angle(VEC3_AXIS_Y, 1.7) },
		{ .type = 0, .mesh = MESH_WALL, .shader = SHADER_WALL, .scale = wall_scale, .color = {},
		  .position = { 0, wallext * 5, 0}, .rotation = quaternion_axis_angle(VEC3_AXIS_Y, 1.1) },
		{ .type = 0, .mesh = MESH_WALL, .shader = SHADER_WALL, .scale = wall_scale, .color = {},
		  .position = { 0, wallext * 6, 0}, .rotation = quaternion_axis_angle(VEC3_AXIS_Y, 2.1) },
		{ .type = 0, .mesh = MESH_WALL, .shader = SHADER_WALL, .scale = wall_scale, .color = {},
		  .position = { 0, wallext * 7, 0}, .rotation = quaternion_axis_angle(VEC3_AXIS_Y, 1.2) },
		{ .type = 0, .mesh = MESH_WALL, .shader = SHADER_WALL, .scale = wall_scale, .color = {},
		  .position = { 0, wallext * 8, 0}, .rotation = quaternion_axis_angle(VEC3_AXIS_Y, 3.4) },
		{ .type = 0, .mesh = MESH_WALL, .shader = SHADER_WALL, .scale = wall_scale, .color = {},
		  .position = { 0, wallext * 9, 0}, .rotation = quaternion_axis_angle(VEC3_AXIS_Y, 0.1) },
	};
	struct scene scene = {
		.count = ARRAY_LEN(level_1),
		.entity = level_1,
	};
	vec3 cap = {0, wallext * 10, 0};
	vec3 cam;
	vec3 pos = game_state->player_pos;
	vec3 spd = (vec3){0, -250, 0};
	vec3 inc = vec3_mult(dt, spd);
	vec3 aim = game_state->player_aim;
	vec3 aim_inc = { 0 };
	float aim_spd = 6;
	struct rock *rocks = game_state->rocks;

	if (key_pressed(input, 'A') || key_pressed(input, KEY_LEFT)) {
		aim_inc.x += dt * aim_spd;
	}
	else if (key_pressed(input, 'D') || key_pressed(input, KEY_RIGHT)) {
		aim_inc.x -= dt * aim_spd;
	}
	if (key_pressed(input, 'W') || key_pressed(input, KEY_UP)) {
		aim_inc.z += dt * aim_spd;
	}
	else if (key_pressed(input, 'S') || key_pressed(input, KEY_DOWN)) {
		aim_inc.z -= dt * aim_spd;
	}
	if (aim_inc.x != 0 || aim_inc.z != 0)
		aim_inc = vec3_normalize(aim_inc);
	aim = vec3_mult(0.9, aim);
	aim = vec3_add(aim, vec3_mult(dt * aim_spd, aim_inc));
	aim.y = 0;

	pos = vec3_add(pos, vec3_mult(game_state->player_speed, aim));
	float posy = pos.y;
	pos.y = 0;
	float wall_radius = 25;
	if (vec3_norm(pos) > wall_radius)
		pos = vec3_mult(wall_radius, vec3_normalize(pos));

	pos.y = posy + inc.y;

	vec3 cam_look = vec3_add(pos, vec3_mult(0.2, aim));
	cam_look.y = pos.y - 5;

	cam = pos;
	cam.y += 3;
	camera_set(&game_state->cam, cam, QUATERNION_IDENTITY);
	camera_look_at(&game_state->cam, cam_look, VEC3_AXIS_Z);

	vec3 rot_dir = {cam_look.x - pos.x, -0.8, 1};
	quaternion player_look = quaternion_look_at(rot_dir, VEC3_AXIS_Y);

	/* end cap position */
	cap.y = pos.y - cap.y;

	/* render */	
	render_scene(game_state, game_asset, &scene, rqueue);
	for (int i = 0; i < 10; i++) {
		if (rocks[i].vld) {
		render_queue_push(rqueue, &(struct entity){
				.type = 0,
				.shader = SHADER_WALL,
				.mesh = MESH_ROCK,
				.scale = {3.1,3.1,3.1},
				.position = rocks[i].pos,
				.rotation = rocks[i].dir,
			});
		/* rock position -> player pos */
		vec3 r2p = vec3_sub(pos, rocks[i].pos);
		/* rock extent */
		vec3 rdir = quaternion_rotate(rocks[i].dir, VEC3_AXIS_Y);
		float lbda = vec3_dot(r2p, rdir);
		vec3 dis = vec3_mult(lbda, rdir);
		float d = vec3_norm(vec3_sub(r2p, dis));

		if (lbda < 25 && d < 6) {
			/* dead */
			game_state->crash_sampler[0].trig_on = 1;
			game_state->new_state = GAME_MENU;
		} else if (lbda < 30 && d < 10) {
			/* trigg sound */
			if (rocks[i].trg == 0) {
				size_t sampler_id = rand() % 4;
				game_state->woosh_sampler[sampler_id].trig_on = 1;
				rocks[i].trg = 1;
			}
		}

		if (lbda < 25) {
			/* in cylindre segment */
			render_queue_push(rqueue, &(struct entity){
				.type = ENTITY_DEBUG,
				.shader = SHADER_SOLID,
				.mesh = DEBUG_MESH_CROSS,
				.scale = {5, 5, 5},
				.position = vec3_add(rocks[i].pos, dis),
				.rotation = QUATERNION_IDENTITY,
				.color = {1,0,0},
				.mode = GL_LINE,
			});
		}	
		render_queue_push(rqueue, &(struct entity){
				.type = ENTITY_DEBUG,
				.shader = SHADER_SOLID,
				.mesh = DEBUG_MESH_CYLINDER,
				.scale = {4, 25, 4},
				.position = rocks[i].pos,
				.rotation = rocks[i].dir,
				.color = {0,1,d < 4?1:0},
				.mode = GL_LINE,
			});
		}
		vec3 rpos = rocks[i + 10].pos;
		rpos.y -= wallext * 10;
		if (rocks[i + 10].vld)
		render_queue_push(rqueue, &(struct entity){
				.type = 0,
				.shader = SHADER_WALL,
				.mesh = MESH_ROCK,
				.scale = {3.1,3.1,3.1},
				.position = rpos,
				.rotation = rocks[i + 10].dir,
			});
	}
	render_queue_push(rqueue, &(struct entity){
			.type = 0,
			.shader = SHADER_WALL,
			.mesh = MESH_CAP,
			.scale = wall_scale,
			.position = cap,
			.rotation = QUATERNION_IDENTITY,
		});
	render_queue_push(rqueue, &(struct entity){
			.type = 0,
			.shader = SHADER_WALL,
			.mesh = MESH_PLAYER,
			.scale = {0.25, 0.25, 0.25},
			.position = pos,
			.rotation = player_look,
		});
	render_queue_push(rqueue, &(struct entity){
			.type = ENTITY_DEBUG,
			.shader = SHADER_SOLID,
			.mesh = DEBUG_MESH_CROSS,
			.scale = {0.1,0.1,0.1},
			.position = pos,
			.rotation = QUATERNION_IDENTITY,
			.color = {0,1,0},
		});
	render_queue_push(rqueue, &(struct entity){
			.type = ENTITY_DEBUG,
			.shader = SHADER_SOLID,
			.mesh = DEBUG_MESH_CROSS,
			.scale = {0.1,0.1,0.1},
			.position = cam_look,
			.rotation = QUATERNION_IDENTITY,
			.color = {1,0,0},
		});

	/* wrap position */
	if (pos.y < 0) {
		int lvl = MIN(game_state->round, 10);
		pos.y = wallext * 10;
		game_state->round++;
		for (int i = 1; i < lvl; i++) {
			float rr = wall_radius + 5;
			float a = 2 * M_PI * rand() / (float) RAND_MAX;
			float x = rr * sin(a);
			float z = rr * cos(a);
			vec3 rpos = {x, wallext * i, z};
			vec3 ldir = rpos;//{0, rpos.y, 0};
			quaternion rdir = quaternion_look_at(ldir, VEC3_AXIS_Y);
			rpos.y = wallext * i;
			rocks[i] = rocks[i + 10];
			rocks[i + 10].vld = 1;
			rocks[i + 10].trg = 0;
			rocks[i + 10].pos = rpos;
			rocks[i + 10].dir = rdir;
		}
	}
	game_state->player_pos = pos;
	game_state->player_aim = aim;
}

void
game_step(struct game_memory *memory, struct input *input, struct audio *audio)
{
	struct game_state *game_state = memory->state.base;
	struct game_asset *game_asset = memory->asset.base;
	struct render_queue rqueue;
	float dt = input->time - game_state->last_time;
	game_state->last_time = input->time;

	memory->scrap.used = 0;
	render_queue_init(&rqueue, game_state, game_asset,
			  mempush(&memory->scrap, SZ_4M), SZ_4M);

	if (game_state->input.width != input->width ||
	    game_state->input.height != input->height) {
		glViewport(0, 0, input->width, input->height);
		camera_set_ratio(&game_state->cam, (float)input->width / (float)input->height);
		game_state->input.width = input->width;
		game_state->input.height = input->height;
	}
	if (key_pressed(input, KEY_ESCAPE))
		game_state->new_state = GAME_MENU;
	if (key_pressed(input, 'X') && !game_state->key_debug)
		game_state->debug = !game_state->debug;
	game_state->key_debug = key_pressed(input, 'X');
	if (key_pressed(input, 'Z') && !game_state->key_flycam) {
		game_state->flycam = !game_state->flycam;
		game_state->window_io->cursor(!game_state->flycam);
	}
	game_state->key_flycam = key_pressed(input, 'Z');

	if (game_state->state != game_state->new_state)
		game_enter_state(game_state, game_state->new_state);

	switch (game_state->state) {
	case GAME_MENU:
		game_menu(game_state, input, &rqueue);
		break;
	case GAME_PLAY:
		game_play(game_state, input, dt, &rqueue);
		break;
	default:
		break;
	}
	if (game_state->debug)
		debug_origin_mark(&rqueue);
	if (game_state->flycam)
		flycam_move(game_state, input, dt);

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	render_queue_exec(&rqueue);

	/* audio */
	float sample_l, sample_r;
	float volume = 0.2;
	for (int i = 0; i < audio->size; i++) {
		if (game_state->state == GAME_PLAY) {
			sample_l = step_sampler(&game_state->wind_sampler);
			sample_r = step_sampler(&game_state->wind_sampler);
			for (size_t i = 0; i<4; i++) {
				sample_l += step_sampler(&game_state->woosh_sampler[i]);
				sample_r += step_sampler(&game_state->woosh_sampler[i]);
				sample_l += step_sampler(&game_state->crash_sampler[i]);
				sample_r += step_sampler(&game_state->crash_sampler[i]);
			}
		} else {
			sample_l = step_sampler(&game_state->theme_sampler);
			sample_r = step_sampler(&game_state->theme_sampler);
			sample_l += step_sampler(&game_state->casey_sampler);
			sample_r += step_sampler(&game_state->casey_sampler);
			sample_l += step_sampler(&game_state->menu_sampler);
			sample_r += step_sampler(&game_state->menu_sampler);
		}
		audio->buffer[i].r = volume * sample_r;
		audio->buffer[i].l = volume * sample_l;
	}

	game_asset_poll(game_asset);
}
