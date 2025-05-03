#include <string.h>
#include "map.h"
#include "alloc.h"
#include "utils.h"
#include "render.h"
#include "engine.h"

// マップアニメーション定義構造体
// 【C言語テクニック】可変長配列を含む構造体
struct map_anim_def_t {
	float inv_frame_time;      // フレーム時間の逆数（計算の最適化用）
	uint16_t sequence_len;     // アニメーションシーケンスの長さ
	uint16_t sequence[];       // タイルインデックスのシーケンス（可変長配列）
};

// データからマップを作成
// 【C言語テクニック】bump_allocによるゲーム用メモリ管理
map_t *map_with_data(uint16_t tile_size, vec2i_t size, uint16_t *data) {
	// ゲームプレイ中にマップを作成できないようにする安全対策
	error_if(engine_is_running(), "Cannot create map during gameplay");

	// マップ構造体のメモリを確保
	map_t *map = bump_alloc(sizeof(map_t));
	map->size = size;
	map->tile_size = tile_size;
	map->distance = 1;  // デフォルトの距離値
	
	// データが提供されていない場合は新しいメモリを割り当て
	map->data = data ? data : bump_alloc(size.x * size.y * sizeof(uint16_t));
	return map;
}

// JSONからマップを読み込み
// 【C言語テクニック】JSONパーサーとデータマッピング
map_t *map_from_json(json_t *def) {
	error_if(engine_is_running(), "Cannot create map during gameplay");

	map_t *map = bump_alloc(sizeof(map_t));

	// JSONからマッププロパティを読み込み
	map->size.x = json_number(json_value_for_key(def, "width"));
	map->size.y = json_number(json_value_for_key(def, "height"));
	map->tile_size = json_number(json_value_for_key(def, "tilesize"));
	map->distance = json_number(json_value_for_key(def, "distance"));
	map->foreground = json_bool(json_value_for_key(def, "foreground"));
	error_if(map->distance == 0, "invalid distance for map");
	map->repeat = json_bool(json_value_for_key(def, "repeat"));

	// マップ名を読み込み
	json_t *name = json_value_for_key(def, "name");
	if (name && name->type == JSON_STRING) {
		error_if(name->len > 15, "Map name exceeds 15 chars: %s", name->string);
		strcpy(map->name, name->string);
	}

	// タイルセット画像を読み込み
	char *tileset_name = json_string(json_value_for_key(def, "tilesetName"));
	if (tileset_name && tileset_name[0]) {
		printf("loaded map %d %d %s\n", map->size.x, map->size.y, tileset_name);
		map->tileset = image(tileset_name);
	}

	// タイルデータを読み込み
	json_t *data = json_value_for_key(def, "data");
	error_if(data->type != JSON_ARRAY, "Map data is not an array");
	error_if(data->len != map->size.y, "Map data height is %d expected %d", data->len, map->size.y);

	// タイルデータ用のメモリを確保
	map->data = bump_alloc(sizeof(uint16_t) * map->size.x * map->size.y);

	// 2次元JSONデータを1次元配列に変換
	// 【C言語テクニック】2D座標から1Dインデックスへの変換
	int index = 0;
	for (int y = 0; y < data->len; y++) {
		json_t *row = json_value_at(data, y);
		for (int x = 0; x < row->len; x++, index++) {
			map->data[index] = json_number(json_value_at(row, x));
			map->max_tile = max(map->max_tile, map->data[index]);
		}
	}	

	return map;
}

// タイルアニメーションを設定
// 【C言語テクニック】可変長構造体のメモリ割り当て
void map_set_anim_with_len(map_t *map, uint16_t tile, float frame_time, uint16_t *sequence, uint16_t sequence_len) {
	error_if(engine_is_running(), "Cannot set map animation during gameplay");
	error_if(sequence_len == 0, "Map animation has empty sequence");

	// マップに存在しないタイルのアニメーションは設定しない
	if (tile > map->max_tile) {
		return;
	}

	// アニメーション配列が未初期化の場合は初期化
	if (!map->anims) {
		map->anims = bump_alloc(sizeof(anim_def_t *) * map->max_tile);
	}

	// アニメーション定義メモリを確保（可変長配列を含む）
	// 【C言語テクニック】構造体サイズと追加データを合わせたメモリ割り当て
	map_anim_def_t *def = bump_alloc(sizeof(map_anim_def_t) + sizeof(uint16_t) * sequence_len);
	def->inv_frame_time = 1.0 / frame_time;  // 逆数を事前計算（最適化）
	def->sequence_len = sequence_len;
	memcpy(def->sequence, sequence, sequence_len * sizeof(uint16_t));
	map->anims[tile] = def;
}

// タイル座標でタイルを取得
// 【C言語テクニック】範囲チェックと2D→1D変換
int map_tile_at(map_t *map, vec2i_t tile_pos) {
	// 境界チェック - 範囲外は0（空）を返す
	if (
		tile_pos.x < 0 || tile_pos.x >= map->size.x ||
		tile_pos.y < 0 || tile_pos.y >= map->size.y
	) {
		return 0;
	}
	else {
		// 2D座標から1D配列インデックスへの変換
		return map->data[tile_pos.y * map->size.x + tile_pos.x];
	}
}

// ピクセル座標でタイルを取得
// 【C言語テクニック】座標変換の抽象化
int map_tile_at_px(map_t *map, vec2_t px_pos) {
	// ピクセル座標からタイル座標への変換
	vec2i_t tile_pos = vec2i_divi(vec2i_from_vec2(px_pos), map->tile_size);
	return map_tile_at(map, tile_pos);
}

// 単一タイルの描画（内部関数）
// 【C言語テクニック】static inlineによる最適化
static inline void map_draw_tile(map_t *map, uint16_t tile, vec2_t pos) {
	// タイルにアニメーションがあればアニメーションフレームを計算
	if (map->anims && map->anims[tile]) {
		map_anim_def_t *def = map->anims[tile];
		// 現在時間からアニメーションフレームを計算
		int frame = (int)(engine.time * def->inv_frame_time) % def->sequence_len;
		tile = def->sequence[frame];
	}

	// タイルセットから指定されたタイルを描画
	image_draw_tile(map->tileset, tile, vec2i(map->tile_size, map->tile_size), pos);
}

// マップの描画
// 【C言語テクニック】効率的な描画のためのビューポートカリング
void map_draw(map_t *map, vec2_t offset) {
	error_if(!map->tileset, "Cannot draw map without tileset");

	// パララックス効果のために距離でオフセットを調整
	offset = vec2_divf(offset, map->distance);
	vec2i_t rs = render_size();
	int ts = map->tile_size;

	// リピートマップの描画（無限スクロール）
	if (map->repeat) {
		// 【C言語テクニック】タイル繰り返し座標の効率的な計算
		vec2i_t tile_offset = vec2i_divi(vec2i_from_vec2(offset), ts);
		vec2_t px_offset = vec2(fmodf(offset.x, ts), fmodf(offset.y, ts));
		
		// 画面に表示されるタイルの範囲を計算（少し余裕を持たせる）
		vec2_t px_min = vec2(-px_offset.x - ts, -px_offset.y - ts);
		vec2_t px_max = vec2(-px_offset.x + rs.x + ts, -px_offset.y + rs.y + ts);

		// 画面に表示されるタイルをループ処理
		vec2_t pos = px_min;
		for (int map_y = -1; pos.y < px_max.y; map_y++, pos.y += ts) {
			// マップ座標をラップアラウンド（循環）させる
			// 【C言語テクニック】負の数のモジュロ演算を正しく処理
			int y = ((map_y + tile_offset.y) % map->size.y + map->size.y) % map->size.y;
			
			pos.x = px_min.x;
			for (int map_x = -1; pos.x < px_max.x; map_x++, pos.x += ts) {
				int x = ((map_x + tile_offset.x) % map->size.x + map->size.x) % map->size.x;
				
				uint16_t tile = map->data[y * map->size.x + x];

				// 0でないタイルのみ描画（0は空タイル）
				if (tile > 0) {
					map_draw_tile(map, tile-1, pos);
				}
			}
		}
	}
	// 非リピートマップの描画
	else {
		// 表示される最小・最大タイル座標の計算（ビューポートカリング）
		// 【C言語テクニック】描画範囲の最適化
		vec2i_t tile_min = vec2i(
			max(0, offset.x / ts),
			max(0, offset.y / ts)
		);
		vec2i_t tile_max = vec2i(
			min(map->size.x, (offset.x + rs.x + ts) / ts),
			min(map->size.y, (offset.y + rs.y + ts) / ts)
		);

		// 表示範囲内のタイルのみを描画
		for (int y = tile_min.y; y < tile_max.y; y++) {
			for (int x = tile_min.x; x < tile_max.x; x++) {
				uint16_t tile = map->data[y * map->size.x + x];
				if (tile > 0) {
					// タイル座標からスクリーン座標への変換
					vec2_t pos = vec2_sub(vec2(x * ts, y * ts), offset);
					map_draw_tile(map, tile-1, pos);
				}
			}
		}
	}
}