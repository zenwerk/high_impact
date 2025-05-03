#ifndef HI_ENGINE_H
#define HI_ENGINE_H

// エンジンはゲームの中心的な機能です。毎フレーム、シーンを更新し、
// すべてのエンティティを更新し、画面全体を描画します。

// エンジンは時間管理、複数の背景マップ、コリジョンマップ、
// その他のグローバル状態を管理します。high_impactでは、engine_tの
// インスタンスは1つだけで、グローバル変数`engine`として利用できます。

#include "types.h"
#include "map.h"


// 1フレームから次のフレームまでの最大時間差（秒）。
// この値を超える場合、不正確な大きな時間ステップを避けるために
// ゲームは遅くなります。
#if !defined(ENGINE_MAX_TICK)
	#define ENGINE_MAX_TICK 0.1
#endif

// 背景マップの最大数
#if !defined(ENGINE_MAX_BACKGROUND_MAPS)
	#define ENGINE_MAX_BACKGROUND_MAPS 4
#endif


// ゲーム内の各シーンは、そのエントリー関数を指定するscene_tを提供する必要があります。
typedef struct {
	// シーンが設定されたときに1回呼び出されます。リソースをロードし、
	// 初期エンティティをインスタンス化するために使用します。
	void (*init)(void);

	// フレームごとに1回呼び出されます。ゲーム固有のロジックを更新するために
	// 使用します。この関数を使用する場合は、どこかでscene_base_update()を
	// 呼び出す必要があるでしょう。
	void (*update)(void);

	// フレームごとに1回呼び出されます。背景やHUDなどを描画するために
	// 使用します。この関数を使用する場合は、どこかでscene_base_draw()を
	// 呼び出す必要があるでしょう。
	void (*draw)(void);

	// 次のシーンが設定されるか、ゲームが終了する前に1回呼び出されます。
	void (*cleanup)(void);
} scene_t;

typedef struct {
	// The real time in seconds since program start
	double time_real;

	// The game time in seconds since scene start
	double time;

	// A global multiplier for how fast game time should advance. Default: 1.0
	double time_scale;

	// The time difference in seconds from the last frame to the current. 
	// Typically 0.01666 (assuming 60hz)
	double tick;

	// The frame number in this current scene. Increases by 1 for every frame.
	uint64_t frame;

	// The map to use for entity vs. world collisions. Reset for each scene.
	// Use engine_set_collision_map() to set it.
	map_t *collision_map;

	// The maps to draw. Reset for each scene. Use engine_add_background_map()
	// to add.
	map_t *background_maps[ENGINE_MAX_BACKGROUND_MAPS];
	uint32_t background_maps_len;

	// A global multiplier that affects the gravity of all entities. This only
	// makes sense for side view games. For a top-down game you'd want to have 
	// it at 0.0. Default: 1.0
	float gravity;

	// The top left corner of the viewport. Internally just an offset when 
	// drawing background_maps and entities.
	vec2_t viewport;

	// Various infos about the last frame
	struct {
		int entities;
		int checks;
		int draw_calls;
		float update;
		float draw;
		float total;
	} perf;
} engine_t;

extern engine_t engine;

// Makes the scene_the current scene. This calls scene->cleanup() on the old
// scene and scene->init() on the new one. The actual swap of scenes happens
// at the beginning of the next frame, so it's ok to call engine_set_scene()
// from the middle of a frame.
// Your main_init() function must call engine_set_scene() to set the initial
// first scene.
void engine_set_scene(scene_t *scene);

// Load a level (background maps, collision map and entities) from a json path.
// This should only be called from within your scenes init() function.
void engine_load_level(char *json_path);

// Add a background map; typically done through engine_load_level()
void engine_add_background_map(map_t *map);

// Set the collision map; typically done through engine_load_level()
void engine_set_collision_map(map_t *map);

// Whether the game is running or we are in a loading phase (i.e. when swapping
// scenes)
bool engine_is_running(void);

// Update all entities
void scene_base_update(void);

// Draw all background maps and entities
void scene_base_draw(void);

// The following functions are automatically called by the platform. No need
// to call yourself.
void engine_init(void);
void engine_update(void);
void engine_cleanup(void);
void engine_resize(vec2i_t size);


#endif
