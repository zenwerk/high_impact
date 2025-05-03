#include "image.h"
#include "alloc.h"
#include "render.h"
#include "utils.h"
#include "engine.h"
#include "platform.h"

// QOIライブラリの設定
// 【C言語テクニック】ヘッダーオンリーライブラリの構成
#define QOI_IMPLEMENTATION   // 実装コードを含める
#define QOI_NO_STDIO         // 標準入出力を使用しない
#define QOI_MALLOC temp_alloc // メモリ割り当てにエンジン独自の関数を使用
#define QOI_FREE temp_free    // メモリ解放にエンジン独自の関数を使用
#include "../libs/qoi.h"      // QOI（Quite OK Image format）ライブラリ

// 画像構造体の完全定義
// 【C言語テクニック】ヘッダーでは不完全型として宣言し、実装で完全定義
struct image_t {
	vec2i_t size;      // 画像サイズ（ピクセル単位）
	texture_t texture; // レンダラー用テクスチャハンドル
};

// 画像リソース管理用のグローバル変数
// 【C言語テクニック】静的配列によるリソース管理
static image_t images[IMAGE_MAX_SOURCES] = {};         // 画像インスタンスの配列
static char *image_paths[IMAGE_MAX_SOURCES] = {};      // 対応するパスの配列
static uint32_t images_len = 0;                        // 現在読み込まれている画像数
static char *image_internal_path = "__internal";       // 内部生成画像の識別子


// 現在の画像リソース状態をマーク
// 【C言語テクニック】シーン管理のためのリソースマーキング
image_mark_t images_mark(void) {
	return (image_mark_t){.index = images_len};
}

// 指定されたマークまで画像リソースをリセット
// シーン切り替え時などに使用
void images_reset(image_mark_t mark) {
	images_len = mark.index;
}

// ピクセル配列から画像を作成
// 【C言語テクニック】メモリからのリソース生成
image_t *image_with_pixels(vec2i_t size, rgba_t *pixels) {
	// リソース制限チェック
	error_if(images_len >= IMAGE_MAX_SOURCES, "Max images (%d) reached", IMAGE_MAX_SOURCES);
	// 実行時安全性チェック
	error_if(engine_is_running(), "Cannot create image during gameplay");

	// 内部生成画像として登録
	image_paths[images_len] = image_internal_path;

	// 画像インスタンスを初期化
	image_t *img = &images[images_len];
	img->size = size;
	img->texture = texture_create(size, pixels);

	// 画像カウンタを増やして新しい画像を返す
	images_len++;
	return img;
}

// QOIファイルから画像を読み込み
// 【C言語テクニック】リソースキャッシュ機能
image_t *image(char *path) {
	// キャッシュ検索：既に読み込まれている場合はそれを返す
	// 【C言語テクニック】キャッシュによるパフォーマンス最適化
	for (uint32_t i = 0; i < images_len; i++) {
		if (str_equals(path, image_paths[i])) {
			return &images[i];
		}
	}

	// 新しい画像の読み込み前のチェック
	error_if(images_len >= IMAGE_MAX_SOURCES, "Max images (%d) reached", IMAGE_MAX_SOURCES);
	error_if(engine_is_running(), "Cannot load image during gameplay");

	// パス文字列をコピー（永続メモリに保存）
	image_paths[images_len] = bump_alloc(strlen(path)+1);
	strcpy(image_paths[images_len], path);

	// プラットフォーム層からファイルデータを読み込み
	// 【C言語テクニック】抽象化されたファイル操作
	uint32_t file_size;
	uint8_t *data = platform_load_asset(path, &file_size);
	error_if(data == NULL, "Failed to load image %s", path);

	// QOIデコーダーでピクセルデータに変換
	qoi_desc desc;
	rgba_t *pixels = qoi_decode(data, file_size, &desc, 4);
	error_if(pixels == NULL, "Failed to decode image: %s", path);
	temp_free(data);  // 元のファイルデータは不要になったので解放

	// 画像インスタンスを初期化
	vec2i_t size = vec2i(desc.width, desc.height);
	image_t *img = &images[images_len];
	img->size = size;
	img->texture = texture_create(size, pixels);

	// 画像カウンタを増加
	images_len++;

	// デコードされたピクセルデータは不要になったので解放
	temp_free(pixels);
	return img;
}

// 画像のサイズを取得
vec2i_t image_size(image_t *img) {
	return img->size;
}

// 画像全体を指定位置に描画
// 【C言語テクニック】単純なケースのラッパー関数
void image_draw(image_t *img, vec2_t pos) {
	vec2_t size = vec2_from_vec2i(img->size);
	render_draw(pos, size, img->texture, vec2(0, 0), size, rgba_white());
}

// 画像の部分領域を描画（拡張版）
// 【C言語テクニック】低レベルレンダリング関数へのブリッジ
void image_draw_ex(image_t *img, vec2_t src_pos, vec2_t src_size, vec2_t dst_pos, vec2_t dst_size, rgba_t color) {
	render_draw(dst_pos, dst_size, img->texture, src_pos, src_size, color);
}

// タイル画像から単一タイルを描画
// 【C言語テクニック】共通利用パターンの簡略化
void image_draw_tile(image_t *img, uint32_t tile, vec2i_t tile_size, vec2_t dst_pos) {
	image_draw_tile_ex(img, tile, tile_size, dst_pos, false, false, rgba_white());
}

// タイル画像から単一タイルを描画（拡張版：反転と色指定可能）
// 【C言語テクニック】スプライトシートからの効率的なタイル抽出
void image_draw_tile_ex(image_t *img, uint32_t tile, vec2i_t tile_size, vec2_t dst_pos, bool flip_x, bool flip_y, rgba_t color) {
	// タイルインデックスから元画像上の位置を計算
	// 【C言語テクニック】1次元インデックスから2次元座標への変換
	vec2_t src_pos = vec2(
		(tile * tile_size.x) % img->size.x,  // X位置：行内のオフセット
		((tile * tile_size.x) / img->size.x) * tile_size.y  // Y位置：行数 * タイル高さ
	);
	vec2_t src_size = vec2(tile_size.x, tile_size.y);
	vec2_t dst_size = src_size;

	// 水平方向の反転処理
	if (flip_x) {
		src_pos.x = src_pos.x + tile_size.x;
		src_size.x = -tile_size.x;  // 負のサイズで反転を表現
	}
	// 垂直方向の反転処理
	if (flip_y) {
		src_pos.y = src_pos.y + tile_size.y;
		src_size.y = -tile_size.y;  // 負のサイズで反転を表現
	}
	
	// 最終的なレンダリング
	render_draw(dst_pos, dst_size, img->texture, src_pos, src_size, color);
}
