#include "engine.h"
#include "input.h"
#include "render.h"
#include "entity.h"
#include "platform.h"
#include "alloc.h"
#include "utils.h"
#include "image.h"
#include "sound.h"

// エンジンのグローバルインスタンスを初期化
engine_t engine = {
	.time_real = 0,          // 実時間（秒）
	.time_scale = 1.0,       // 時間スケール（ゲーム速度の調整用）
	.time = 0,               // ゲーム内時間（秒）
	.tick = 0,               // 前フレームからの経過時間
	.frame = 0,              // フレーム数
	.collision_map = NULL,   // コリジョンマップ
	.gravity = 1.0,          // 重力の強さ
};


// 現在のシーンと次のシーンへの参照
static scene_t *scene = NULL;        // 現在アクティブなシーン
static scene_t *scene_next = NULL;   // 次のフレームで切り替えるシーン

// 初期状態のリソースマーク（リセット時に使用）
static texture_mark_t init_textures_mark;  // テクスチャの初期状態
static image_mark_t init_images_mark;      // 画像の初期状態
static bump_mark_t init_bump_mark;         // メモリ割り当ての初期状態
static sound_mark_t init_sounds_mark;      // サウンドの初期状態

// エンジンの実行状態
static bool is_running = false;      // ゲームが実行中かどうか

// ゲームのメイン関数（外部から実装）
extern void main_init(void);         // ゲーム初期化関数
extern void main_cleanup(void);      // ゲーム終了関数

void engine_init(void) {
	// エンジンの初期化処理
	engine.time_real = platform_now();              // 現在時刻を取得
	render_init(platform_screen_size());            // レンダラーを初期化（画面サイズ指定）
	sound_init(platform_samplerate());              // サウンドシステムを初期化
	platform_set_audio_mix_cb(sound_mix_stereo);    // オーディオミックスコールバックを設定
	input_init();                                   // 入力システムを初期化
	entities_init();                                // エンティティシステムを初期化
	main_init();                                    // ゲーム固有の初期化を実行

	// 初期状態をマーク（後でリセットできるように）
	init_bump_mark = bump_mark();                   // メモリ割り当て状態を記録
	init_images_mark = images_mark();               // 画像リソース状態を記録
	init_sounds_mark = sound_mark();                // サウンドリソース状態を記録
	init_textures_mark = textures_mark();           // テクスチャリソース状態を記録
}

void engine_cleanup(void) {
	// エンジンの終了処理
	entities_cleanup();    // エンティティシステムの後始末
	main_cleanup();        // ゲーム固有の後始末処理
	input_cleanup();       // 入力システムの後始末
	sound_cleanup();       // サウンドシステムの後始末
	render_cleanup();      // レンダラーの後始末
}

void engine_load_level(char *json_path) {
	// JSONからレベルデータを読み込み
	json_t *json = platform_load_asset_json(json_path);
	error_if(!json, "Could not load level json at %s", json_path);  // 読み込み失敗時にエラー

	// 既存のエンティティやマップをリセット
	entities_reset();                   // エンティティをすべて削除
	engine.background_maps_len = 0;     // 背景マップをクリア
	engine.collision_map = NULL;        // コリジョンマップをクリア

	// マップの読み込みと設定
	json_t *maps = json_value_for_key(json, "maps");
	for (int i = 0; maps && i < maps->len; i++) {
		json_t *map_def = json_value_at(maps, i);
		char *name = json_string(json_value_for_key(map_def, "name"));
		map_t *map = map_from_json(map_def);  // JSONからマップを生成

		// マップタイプによる振り分け
		if (str_equals(name, "collision")) {
			engine_set_collision_map(map);    // コリジョンマップとして設定
		}
		else {
			engine_add_background_map(map);   // 背景マップとして追加
		}
	}

	// エンティティの読み込みと設定
	json_t *entities = json_value_for_key(json, "entities");

	// エンティティの設定情報を一時保存
	// すべてのエンティティが生成された後に設定を適用するため
	// FIXME: スタック上に確保しているが、temp_allocを使うべきかも
	struct { entity_t *entity; json_t *settings; } entity_settings[entities->len];
	int entity_settings_len = 0;

	// すべてのエンティティを生成
	for (int i = 0; entities && i < entities->len; i++) {
		json_t *def = json_value_at(entities, i);
		
		// エンティティタイプの取得と検証
		char *type_name = json_string(json_value_for_key(def, "type"));
		error_if(!type_name, "Entity has no type");  // タイプがない場合はエラー
		
		entity_type_t type = entity_type_by_name(type_name);
		error_if(!type, "Unknown entity type %s", type_name);  // 未知のタイプの場合はエラー

		// エンティティの位置を取得
		vec2_t pos = {
			json_number(json_value_for_key(def, "x")),
			json_number(json_value_for_key(def, "y"))
		};

		// エンティティを生成
		entity_t *ent = entity_spawn(type, pos);
		json_t *settings = json_value_for_key(def, "settings");
		if (ent && settings && settings->type == JSON_OBJECT) {

			// 名前があれば設定
			json_t *name = json_value_for_key(settings, "name");
			if (name && name->type == JSON_STRING) {
				ent->name = bump_alloc(name->len + 1);  // 名前用のメモリを確保
				strcpy(ent->name, name->string);        // 名前をコピー
			}

			// 設定情報を一時保存
			entity_settings[entity_settings_len].entity = ent;
			entity_settings[entity_settings_len].settings = settings;
			entity_settings_len++;
		}
	}

	// 保存しておいた設定をすべてのエンティティに適用
	for (int i = 0; i < entity_settings_len; i++) {
		entity_settings(entity_settings[i].entity, entity_settings[i].settings);
	}
	
	// 一時的に使用したJSONを解放
	temp_free(json);
}

void engine_add_background_map(map_t *map) {
	// 背景マップを追加（最大数を超えるとエラー）
	error_if(engine.background_maps_len >= ENGINE_MAX_BACKGROUND_MAPS, "BACKGROUND_MAPS_MAX reached");
	engine.background_maps[engine.background_maps_len++] = map;  // マップを追加して数をカウントアップ
}

void engine_set_collision_map(map_t *map) {
	// コリジョンマップを設定
	engine.collision_map = map;
}

void engine_set_scene(scene_t *scene) {
	// 次のシーンを設定（実際の切り替えは次のフレーム開始時）
	scene_next = scene;
}

void engine_update(void) {
	// フレーム開始時間を記録
	double time_frame_start = platform_now();

	// シーン切り替えが要求されている場合の処理
	if (scene_next) {
		is_running = false;  // 実行中フラグをオフに
		
		// 現在のシーンのクリーンアップを実行（存在する場合）
		if (scene && scene->cleanup) {
			scene->cleanup();
		}

		// すべてのリソースを初期状態にリセット
		textures_reset(init_textures_mark);  // テクスチャをリセット
		images_reset(init_images_mark);      // 画像をリセット
		sound_reset(init_sounds_mark);       // サウンドをリセット
		bump_reset(init_bump_mark);          // メモリをリセット
		entities_reset();                    // エンティティをリセット

		// エンジン状態をリセット
		engine.background_maps_len = 0;      // 背景マップをクリア
		engine.collision_map = NULL;         // コリジョンマップをクリア
		engine.time = 0;                     // ゲーム内時間をリセット
		engine.frame = 0;                    // フレーム数をリセット
		engine.viewport = vec2(0, 0);        // ビューポートをリセット

		// 新しいシーンを設定して初期化
		scene = scene_next;
		if (scene->init) {
			scene->init();  // シーンの初期化関数を実行
		}
		scene_next = NULL;  // 次のシーン参照をクリア
	}
	is_running = true;  // 実行中フラグをオンに

	// シーンが設定されていない場合はエラー
	error_if(scene == NULL, "No scene set");

	// 時間計算と更新
	double time_real_now = platform_now();                             // 現在時刻を取得
	double real_delta = time_real_now - engine.time_real;              // 前フレームからの経過時間
	engine.time_real = time_real_now;                                  // 現在時刻を更新
	engine.tick = min(real_delta * engine.time_scale, ENGINE_MAX_TICK); // フレーム時間を計算（最大値制限あり）
	engine.time += engine.tick;                                        // ゲーム内時間を更新
	engine.frame++;                                                    // フレームカウンタを増加

	// 一時メモリプールを使用してフレーム処理
	alloc_pool() {
		// シーンの更新処理
		if (scene->update) {
			scene->update();  // シーン独自の更新処理
		}
		else {
			scene_base_update();  // 標準の更新処理
		}

		// 更新処理の時間を記録
		engine.perf.update = platform_now() - time_real_now;
		
		// 描画準備
		render_frame_prepare();

		// シーンの描画処理
		if (scene->draw) {
			scene->draw();  // シーン独自の描画処理
		}
		else {
			scene_base_draw();  // 標準の描画処理
		}
		
		// 描画終了処理
		render_frame_end();
		
		// 描画処理の時間を記録
		engine.perf.draw = (platform_now() - time_real_now) - engine.perf.update;
	}

	// フレーム終了処理
	input_clear();        // 入力状態をクリア
	temp_alloc_check();   // 一時メモリリークをチェック

	// パフォーマンス統計を更新
	engine.perf.draw_calls = render_draw_calls();               // 描画コール数を記録
	engine.perf.total = platform_now() - time_frame_start;      // フレーム全体の処理時間を記録
}

bool engine_is_running(void) {
	// ゲームが実行中かどうかを返す
	return is_running;
}

void engine_resize(vec2i_t size) {
	// 画面サイズが変更された場合にレンダラーをリサイズ
	render_resize(size);
}


void scene_base_update(void) {
	// 標準のシーン更新処理（すべてのエンティティを更新）
	entities_update();
}

void scene_base_draw(void) {
	// ビューポートをピクセル単位にスナップ
	vec2_t px_viewport = render_snap_px(engine.viewport);
	
	// 背景マップの描画（foregroundフラグがfalseのもの）
	for (int i = 0; i < engine.background_maps_len; i++) {
		if (!engine.background_maps[i]->foreground) {
			map_draw(engine.background_maps[i], px_viewport);
		}
	}

	// エンティティの描画
	entities_draw(px_viewport);

	// 前景マップの描画（foregroundフラグがtrueのもの）
	for (int i = 0; i < engine.background_maps_len; i++) {
		if (engine.background_maps[i]->foreground) {
			map_draw(engine.background_maps[i], px_viewport);
		}
	}
}