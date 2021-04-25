#ifndef ASSET_H
#define ASSET_H

enum asset_key {
	DEBUG_MESH_CROSS,
	MESH_FLOOR,
	MESH_WALL,
	MESH_MENU_START,
	MESH_MENU_QUIT,
	SHADER_SOLID,
	SHADER_TEXT,
	SHADER_WALL,
	WAV_NL_SEQ_1,
	ASSET_KEY_COUNT,
};

enum asset_state {
	STATE_UNLOAD,
	STATE_LOADED,
};

struct game_asset {
	struct memory_zone *memzone;
	struct memory_zone  tmpzone;
	struct memory_zone *samples;
	struct file_io *file_io;
	struct res_data {
		enum asset_state state;
		time_t since;
		size_t size;
		void *base;
	} assets[ASSET_KEY_COUNT];
};

void game_asset_init(struct game_asset *game_asset, struct memory_zone *memzone, struct memory_zone *samples, struct file_io *file_io);
void game_asset_fini(struct game_asset *game_asset);
void game_asset_poll(struct game_asset *game_asset);

struct shader *game_get_shader(struct game_asset *game_asset, enum asset_key key);
struct mesh *game_get_mesh(struct game_asset *game_asset, enum asset_key key);
struct wav *game_get_wav(struct game_asset *game_asset, enum asset_key key);

#endif
