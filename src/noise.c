#include "noise.h"
#include "alloc.h"
#include "utils.h"

// ノイズジェネレータの実際の構造体定義
// 【C言語テクニック】不透明ポインタパターンの実装部分です。
// .hファイルではタグ付き型名のみを定義し、実際の構造体定義は.cファイルに隠蔽しています。
struct noise_t {
	int size_bits;    // サイズビット数（格子サイズを決定）
	vec2_t *g;        // 勾配ベクトル配列
	uint16_t *p;      // 順列テーブル（インデックスをシャッフルするため）
};

// ノイズジェネレータを作成して初期化する関数
noise_t *noise(uint8_t size_bits) {
	// サイズの上限チェック（2^15以上のサイズは不可）
	error_if(size_bits > 15, "Max noise size bits");
	
	// ノイズ構造体のメモリを割り当て
	// 【C言語テクニック】バンプアロケータを使用（自動メモリ管理）
	noise_t *n = bump_alloc(sizeof(noise_t));
	n->size_bits = size_bits;

	// 格子サイズを計算（2のべき乗）
	// 【C言語テクニック】ビットシフトによる2の累乗計算（2^size_bits）
	uint16_t size = 1 << size_bits;
	
	// 勾配ベクトルと順列テーブルのメモリを割り当て
	n->g = bump_alloc(sizeof(vec2_t) * size);
	n->p = bump_alloc(sizeof(uint16_t *) * size);

	// 勾配ベクトルをランダムに初期化し、順列テーブルを準備
	for (int i = 0; i < size; i++) {
		// -1〜1の範囲でランダムな方向ベクトルを生成
		n->g[i] = vec2(rand_float(-1, 1), rand_float(-1, 1));
		// 順列テーブルを連番で初期化
		n->p[i] = i;
	}
	
	// 順列テーブルをシャッフル（ランダム化）
	// 【C言語テクニック】Fisher-Yatesアルゴリズムによる配列のシャッフル
	shuffle(n->p, size);
	
	return n;
}

// 指定位置のノイズ値を生成する関数
float noise_gen(noise_t *n, vec2_t pos) {
	// 格子サイズと格子インデックスのマスクを計算
	// 【C言語テクニック】ビットマスクを使った効率的な剰余計算
	// size-1（例：15=0b1111）のビットマスクを使うと、&演算で剰余を高速に計算できます
	int size = 1 << n->size_bits;
	int mask = size - 1;

	// ポインタのローカルコピー（最適化のため）
	uint16_t *p = n->p;
	vec2_t *g = n->g;

	// 使用する勾配を計算
	// 現在の格子点と次の格子点を特定
	int qx0 = (int)pos.x & mask;  // 現在のx格子点
	int qx1 = (qx0 + 1) & mask;   // 次のx格子点（マスクで循環）
	float tx0 = pos.x - qx0;      // 現在格子点からの相対x位置
	float tx1 = tx0 - 1;          // 次格子点からの相対x位置

	int qy0 = (int)pos.y & mask;  // 現在のy格子点
	int qy1 = (qy0 + 1) & mask;   // 次のy格子点（マスクで循環）
	float ty0 = pos.y - qy0;      // 現在格子点からの相対y位置
	float ty1 = ty0 - 1;          // 次格子点からの相対y位置

	// 順列テーブルを使って疑似ランダムに選択された勾配を取得
	// 【C言語テクニック】ハッシュ関数として順列テーブルを2回参照する手法
	int q00 = p[(qy0 + p[qx0]) & mask];  // 左下の格子点の勾配インデックス
	int q01 = p[(qy0 + p[qx1]) & mask];  // 右下の格子点の勾配インデックス
	int q10 = p[(qy1 + p[qx0]) & mask];  // 左上の格子点の勾配インデックス
	int q11 = p[(qy1 + p[qx1]) & mask];  // 右上の格子点の勾配インデックス

	// ベクトルと勾配の内積を計算
	// 【C言語テクニック】ドット積の最適化された計算
	float v00 = g[q00].x * tx0 + g[q00].y * ty0;  // 左下格子点での影響値
	float v01 = g[q01].x * tx1 + g[q01].y * ty0;  // 右下格子点での影響値
	float v10 = g[q10].x * tx0 + g[q10].y * ty1;  // 左上格子点での影響値
	float v11 = g[q11].x * tx1 + g[q11].y * ty1;  // 右上格子点での影響値

	// イージング関数で補間（スムージング）
	// 【C言語テクニック】エルミート補間関数 t^2(3-2t) を使用
	// これにより、格子点間の値がスムーズに変化します
	float wx = (3 - 2 * tx0) * tx0 * tx0;  // xのウェイト計算
	float v0 = v00 - wx*(v00 - v01);       // x方向の補間

	float wy = (3 - 2 * ty0) * ty0 * ty0;  // yのウェイト計算
	float v = v0 - wy*(v0 - v1);           // y方向の補間

	// 最終的なノイズ値を返す（範囲は約-0.7〜0.7）
	return v;
}