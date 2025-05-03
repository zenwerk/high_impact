#ifndef HI_IMAGE_H
#define HI_IMAGE_H

// 画像システム
// 画像はQOIファイルから読み込むか、rgba_tピクセルの配列から直接作成できます。
// 特定のパスの画像が既に読み込まれている場合、同じパスでimage()を呼び出すと
// 同じ画像インスタンスが返されます（キャッシュ機能）。
// 画像は全体を描画したり、一部分だけを描画したり、「タイル」として描画したりできます。
// 【C言語テクニック】リソース管理と再利用のパターン

#include "types.h"

// 同時に読み込まれる画像の最大数
// 【C言語テクニック】条件付きコンパイルによるデフォルト値の設定
#if !defined(IMAGE_MAX_SOURCES)
	#define IMAGE_MAX_SOURCES 1024
#endif

// 前方宣言：実装の詳細を隠蔽
// 【C言語テクニック】不完全型による実装の隠蔽
typedef struct image_t image_t;

// ピクセル配列から画像を作成
// size.x * size.yのサイズを持つピクセル配列から画像を生成
// 【C言語テクニック】メモリからのリソース生成
image_t *image_with_pixels(vec2i_t size, rgba_t *pixels);

// QOIファイルから画像を読み込み
// 同じパスで複数回呼び出すと、同じキャッシュされた画像インスタンスが返される
// 【C言語テクニック】リソースキャッシュ
image_t *image(char *path);

// 画像のサイズを返す
vec2i_t image_size(image_t *img);

// 画像全体を指定位置に描画
// 【C言語テクニック】基本的な描画関数
void image_draw(image_t *img, vec2_t pos);

// 画像の一部（src_pos, src_size）を指定位置（dst_pos）とサイズ（dst_size）で描画し、色調（color）を適用
// 【C言語テクニック】柔軟なパラメータによる拡張機能
void image_draw_ex(image_t *img, vec2_t src_pos, vec2_t src_size, vec2_t dst_pos, vec2_t dst_size, rgba_t color);

// タイルサイズで分割された画像から、単一のタイルを描画
// 【C言語テクニック】タイルベースの描画システム
void image_draw_tile(image_t *img, uint32_t tile, vec2i_t tile_size, vec2_t dst_pos);

// タイルを描画し、X/Y反転と色調を指定
// 【C言語テクニック】追加パラメータによる機能強化
void image_draw_tile_ex(image_t *img, uint32_t tile, vec2i_t tile_size, vec2_t dst_pos, bool flip_x, bool flip_y, rgba_t color);

// エンジンによって画像メモリを管理するために呼び出される関数群
// 【C言語テクニック】シーン切り替え時のリソース管理
typedef struct { uint32_t index; } image_mark_t;

// 現在の画像リソース状態をマーク
image_mark_t images_mark(void);

// 指定されたマークまで画像リソースをリセット
void images_reset(image_mark_t mark);

#endif
