#ifndef HI_TRACE_H
#define HI_TRACE_H

// コリジョン検出システム
// Trace関数は軸に沿った境界ボックス（AABB）をマップ（通常はcollision_map）上で
// 「スイープ」し、衝突が最初に発生した位置と追加情報を返します。
// 【C言語テクニック】スイープ＆プルーン衝突検出アルゴリズム

#include "types.h"
#include "map.h"

// トレース結果構造体
typedef struct {
	// 衝突したタイル。0は衝突なし。
	int tile;

	// 衝突したタイルの位置（タイル空間）
	vec2i_t tile_pos;

	// 正規化された0〜1のトレース長さ。
	// トレースが衝突せずに終了した場合、lengthは1になります。
	// 【C言語テクニック】正規化された値でオブジェクトの動きを制御
	float length;

	// トレースされたAABBの左上隅の結果位置
	vec2_t pos;

	// 衝突した表面の法線ベクトル
	// 【C言語テクニック】衝突応答を計算するために使用
	vec2_t normal;
} trace_t;

// マップ上でAABBの左上隅、移動ベクトル、サイズを使用してトレース（衝突検出）を行う
// 【C言語テクニック】コリジョン検出と応答の分離
trace_t trace(map_t *map, vec2_t from, vec2_t vel, vec2_t size);

#endif
