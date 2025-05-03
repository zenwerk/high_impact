#ifndef HI_MAP_H
#define HI_MAP_H

// タイルマップシステム
// マップはタイルインデックスの配列で構成され、描画やtrace()によるコリジョン判定に
// 使用できます。マップはJSON形式から読み込むか、データ配列から作成できます。
// 【C言語テクニック】タイルベースの2Dゲーム用データ構造

#include "types.h"
#include "../libs/pl_json.h"
#include "image.h"
#include "animation.h"

// マップアニメーション定義の前方宣言
typedef struct map_anim_def_t map_anim_def_t;

// マップ構造体
typedef struct {
	// マップのサイズ（タイル単位）
	vec2i_t size;

	// このマップのタイルサイズ（ピクセル単位）
	uint16_t tile_size;

	// マップの名前。コリジョンマップの場合は通常「collision」。
	// 背景マップは任意の名前を持つことができます。
	char name[16];

	// マップを特定のオフセットで描画する際の「距離」。
	// 距離が大きいマップほど、移動速度が遅くなります（パララックス効果）。
	// デフォルトは1。
	// 【C言語テクニック】パララックススクローリングの簡易実装
	float distance;

	// マップ描画時に無限に繰り返すかどうか
	bool repeat;

	// すべてのエンティティの前面に描画するかどうか
	bool foreground;

	// 描画時に使用するタイルセット画像。
	// コリジョンマップの場合はNULLの場合があります。
	image_t *tileset;

	// 描画時の特定タイルのアニメーション。
	// map_set_anim()を使用してアニメーションを追加します。
	map_anim_def_t **anims;

	// タイルインデックスの配列（長さはsize.x * size.y）
	uint16_t *data;

	// そのマップ内の最大タイルインデックス（内部で使用）
	uint16_t max_tile;
} map_t;

// 指定されたデータでマップを作成します。
// dataがNULLでない場合、少なくともsize.x * size.y要素の長さが必要です。
// データはコピーされません。dataがNULLの場合、十分な長さの配列が割り当てられます。
// 【C言語テクニック】条件付きメモリ割り当て
map_t *map_with_data(uint16_t tile_size, vec2i_t size, uint16_t *data);

// JSONからマップを読み込みます。JSONは以下のレイアウトである必要があります。
// タイルインデックスには+1のバイアスがあります。つまり、インデックス0は何も描画せず、
// 空のタイルを表します。インデックス1はタイルセットの0番目のタイルを描画します。
/*
{
	"name": "background",      // マップ名
	"width": 4,                // 幅（タイル単位）
	"height": 2,               // 高さ（タイル単位）
	"tilesetName": "assets/tiles/biolab.qoi",  // タイルセット画像パス
	"repeat": true,            // 繰り返し描画するか
	"distance": 1.0,           // 距離（パララックス係数）
	"tilesize": 8,             // タイルサイズ（ピクセル単位）
	"foreground": false,       // 前景として描画するか
	"data": [                  // タイルデータ（2次元配列）
		[0,1,2,3],
		[3,2,1,0],
	]
}
*/
// 【C言語テクニック】JSONパーサーを使用したデータロード
map_t *map_from_json(json_t *def);

// 特定のタイルのフレーム時間とアニメーションシーケンスを設定します。
// これはscene_init()内でのみ実行できます。
// 【C言語テクニック】可変引数マクロを使用したAPIの簡素化
#define map_set_anim(MAP, TILE, FRAME_TIME, ...) \
	map_set_anim_with_len(MAP, TILE, FRAME_TIME, (uint16_t[])__VA_ARGS__, len((uint16_t[])__VA_ARGS__))
void map_set_anim_with_len(map_t *map, uint16_t tile, float frame_time, uint16_t *sequence, uint16_t sequence_len);

// タイル位置でのタイルインデックスを返します。
// 範囲外の場合は0を返します。
int map_tile_at(map_t *map, vec2i_t tile_pos);

// ピクセル位置でのタイルインデックスを返します。
// 範囲外の場合は0を返します。
int map_tile_at_px(map_t *map, vec2_t px_pos);

// 指定されたオフセットでマップを描画します。
// これは「距離」（パララックス）を考慮します。
void map_draw(map_t *map, vec2_t offset);

#endif
