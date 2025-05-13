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
	// プログラムが起動してからの実時間
	double time_real;

	// 現在のシーンが開始してからのゲーム時間
	double time;

	// ゲーム時間をどれだけ速く進めるかのグローバルな倍率. デフォルト:1.0
	double time_scale;

	// The time difference in seconds from the last frame to the current. 
	// Typically 0.01666 (assuming 60hz)
	// 最後のフレームから現在までの時間差（秒）。通常は0.01666（60Hzを想定）
	double tick;

	// The frame number in this current scene. Increases by 1 for every frame.
	uint64_t frame;

	// The map to use for entity vs. world collisions. Reset for each scene.
	// Use engine_set_collision_map() to set it.
	// エンティティ対ワールドの衝突に使用するマップ. シーンごとにリセットされる. 設定するにはengine_set_collision_map()を使う.
	map_t *collision_map;

	// The maps to draw. Reset for each scene. Use engine_add_background_map() to add.
	// 描画するマップ. シーンごとにリセットする. 追加するにはengine_add_background_map()を使う.
	map_t *background_maps[ENGINE_MAX_BACKGROUND_MAPS];
	uint32_t background_maps_len;

	// A global multiplier that affects the gravity of all entities. This only
	// makes sense for side view games. For a top-down game you'd want to have 
	// it at 0.0. Default: 1.0
	// すべてのエンティティの重力に影響するグローバルな乗数. これはサイドビューのゲームでのみ意味がある. トップダウンゲームでは0.0にするべき. デフォルト: 1.0
	float gravity;

	// The top left corner of the viewport. Internally just an offset when 
	// drawing background_maps and entities.
	// ビューポートの左上隅。内部的にはbackground_mapやentityを描画する際のオフセットに過ぎない.
	vec2_t viewport;

	// Various infos about the last frame
	// 最後のフレームに関する様々な情報
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
// scene_を現在のシーンにする. これは古いシーンでscene->cleanup()を呼び出し、新しいシーンでscene->init()を呼び出す.
// 実際のシーンの入れ替えは次のフレームの最初に行われるので、フレームの途中からengine_set_scene()を呼び出しても大丈夫.
// main_init()関数は最初のシーンを設定するためにengine_set_scene()を呼び出す必要があある.
void engine_set_scene(scene_t *scene);

// Load a level (background maps, collision map and entities) from a json path.
// This should only be called from within your scenes init() function.
// jsonパスからレベル（バックグラウンドマップ、コリジョンマップ、エンティティ）をロードする. これは、シーンのinit()関数の中からのみ呼び出す必要がある.
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
