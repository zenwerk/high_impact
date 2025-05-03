#ifndef HI_ANIMATION_H
#define HI_ANIMATION_H

// アニメーションシステム
// アニメーションはスプライトシートとして画像(image_t)を使用し、`frame_size`に
// 従ってこのシートを複数のアニメーションフレームに分割します。フレーム番号の
// シーケンスとフレームごとの時間を使用して、描画するフレームを決定します。

// アニメーションは2つの部分に分かれています：
// 1. アニメーション定義(`anim_def_t`) - フレーム数、フレームシーケンス、
//    フレームごとの時間を含みます
// 2. アニメーションインスタンス(`anim_t`) - アニメーションの現在の状態を保持し、
//    描画するフレームを決定します

// 各anim_def_tは、任意の数のanim_tインスタンスで共有できます。
// 【C言語テクニック】これはフライウェイトパターンの実装例です。
// 共通データ(定義)を複数のインスタンスで共有することでメモリ使用量を削減しています。

#include "types.h"
#include "image.h"
#include "engine.h"
#include "utils.h"

// アニメーション定義構造体
// 【C言語テクニック】可変長配列を含む構造体
// sequenceは構造体の末尾に宣言された可変長の配列メンバーです
typedef struct {
	image_t *sheet;        // スプライトシート画像
	vec2i_t frame_size;    // 各フレームのサイズ（ピクセル単位）
	bool loop;             // アニメーションをループするかどうか
	vec2_t pivot;          // 回転の中心点（原点からのオフセット）
	float frame_time;      // 各フレームの表示時間（秒）
	float inv_total_time;  // 全体時間の逆数（最適化用）
	uint16_t sequence_len; // シーケンスの長さ
	uint16_t sequence[];   // フレーム番号のシーケンス（可変長配列）
} anim_def_t;

// アニメーションインスタンス構造体
typedef struct {
	anim_def_t *def;       // このインスタンスが使用するアニメーション定義
	double start_time;     // アニメーションの開始時間
	uint16_t tile_offset;  // タイルオフセット（別のタイルセットを使用する場合）
	bool flip_x;           // X軸方向に反転するかどうか
	bool flip_y;           // Y軸方向に反転するかどうか
	float rotation;        // 回転角度（ラジアン）
	rgba_t color;          // 色調整（乗算）
} anim_t;

// アニメーション停止を示す特殊な値
// 【C言語テクニック】マジックナンバーを定数化（0xffffは最大の16ビット符号なし整数）
#define ANIM_STOP 0xffff

// 指定したシート、フレームサイズ、フレーム時間、シーケンスでanim_defを作成するマクロ
// このマクロはシーケンスの長さを自動的に計算しますが、シーケンスがリテラルとして
// 提供される場合にのみ機能します。
// 例: anim_def(sheet, vec2i(16, 8), 0.5, {0,1,2,3,4});
//
// 【C言語テクニック】可変引数マクロと配列サイズの自動計算を組み合わせています
// __VA_ARGS__は可変長引数を展開し、len()マクロは配列の要素数を計算します
#define anim_def(SHEET, FRAME_SIZE, FRAME_TIME, ...) \
	anim_def_with_len(SHEET, FRAME_SIZE, FRAME_TIME, (uint16_t[])__VA_ARGS__, len((uint16_t[])__VA_ARGS__))


// 指定したシート、フレームサイズ、シーケンス、シーケンス長でanim_defを作成する関数
// 【C言語テクニック】この関数は内部実装で、通常はanim_def()マクロを通して呼び出されます
anim_def_t *anim_def_with_len(image_t *sheet, vec2i_t frame_size, float frame_time, uint16_t *sequence, uint16_t sequence_len);

// 指定したアニメーション定義でanim_tインスタンスを作成するマクロ
// 【C言語テクニック】複合リテラルを使った構造体の初期化。
// .演算子による指定初期化子を使用しています
#define anim(ANIM_DEF)(anim_t){.def = ANIM_DEF, .color = rgba_white(), .start_time = engine.time}

// アニメーションをシーケンスの最初のフレームに巻き戻す
void anim_rewind(anim_t *anim);

// アニメーションをシーケンスのn番目のインデックスに移動する
void anim_goto(anim_t *anim, int frame);

// アニメーションをシーケンスのランダムなフレームに移動する
void anim_goto_rand(anim_t *anim);

// このアニメーションが再生された回数を返す
// 【C言語テクニック】状態計算を関数化することで、複雑なロジックを隠蔽しています
uint32_t anim_looped(anim_t *anim);

// 指定した位置にアニメーションを描画する
void anim_draw(anim_t *anim, vec2_t pos);

#endif
