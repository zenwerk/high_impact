#include "engine.h"
#include "render.h"
#include "alloc.h"
#include "utils.h"
#include "platform.h"

// ソフトウェアレンダラー実装
// =============================================================================
// 【初心者向け解説】
// このファイルは、CPU上で動作するソフトウェアレンダラーを実装しています。
// グラフィックスハードウェア（GPU）を使用せず、CPUだけで描画処理を行います。
//
// 他のレンダラー実装と比較：
// - OpenGL: 広く使われているクロスプラットフォームなAPI（GPU使用）
// - Metal: Apple製品専用の高速なAPI（GPU使用）
// - ソフトウェア: どのような環境でも動作するが、処理速度は遅い（CPU使用）
//
// ソフトウェアレンダラーは最も単純で互換性が高いですが、
// 表示速度や機能は制限されます。主に以下の場合に使用されます：
// - 開発初期段階でのテスト
// - 非常に古いあるいは特殊なハードウェア
// - GPUが使用できない環境

// テクスチャなし用の白テクスチャ
texture_t RENDER_NO_TEXTURE;

// テクスチャ情報の配列
// -----------------------------------------------------------------------------
// 【初心者向け解説】
// 各テクスチャのサイズとピクセルデータを保存する構造体の配列
struct {
	vec2i_t size;   // テクスチャの幅と高さ
	rgba_t *pixels; // ピクセルデータ（RGBA形式）
} textures[RENDER_TEXTURES_MAX];

// 登録済みテクスチャの数
uint32_t textures_len = 0;

// 画面バッファ関連の変数
// -----------------------------------------------------------------------------
// 【初心者向け解説】
// 実際に画面に表示されるピクセルデータとその情報を保持する変数
static rgba_t *screen_buffer;  // 画面ピクセルデータへのポインタ
static int32_t screen_pitch;   // 画面の1行あたりのバイト数
static int32_t screen_ppr;     // 画面の1行あたりのピクセル数（Pixels Per Row）
static vec2i_t screen_size;    // 画面の幅と高さ

// ソフトウェアレンダラーの初期化
// -----------------------------------------------------------------------------
// 【初心者向け解説】
// ソフトウェアレンダラーは非常にシンプルなため、特別な初期化は必要ありません。
// GPUベースのレンダラー（OpenGL、Metal）と違い、特殊なリソースやバッファの
// 割り当ては行いません。
void render_backend_init(void) {
	// 初期化は特に必要なし
}

// ソフトウェアレンダラーのクリーンアップ
// -----------------------------------------------------------------------------
// 【初心者向け解説】
// 終了時の後片付け処理。ソフトウェアレンダラーでは特に必要ありません。
void render_backend_cleanup(void) {
	// クリーンアップは特に必要なし
}

// 画面サイズの設定
// -----------------------------------------------------------------------------
// 【初心者向け解説】
// 画面サイズを設定します。この情報は描画時のクリッピング（画面外の描画を
// 省略すること）に使用されます。
void render_set_screen(vec2i_t size) {
	screen_size = size;  // 画面サイズを保存
}

// ブレンドモードの設定
// -----------------------------------------------------------------------------
// 【初心者向け解説】
// 透明度の計算方法を設定します。現在のソフトウェアレンダラーでは
// 実装されていません（将来の拡張のためのプレースホルダー）。
void render_set_blend_mode(render_blend_mode_t mode) {
	// TODO: 将来実装予定
}

// ポストエフェクトの設定
// -----------------------------------------------------------------------------
// 【初心者向け解説】
// 画面全体に適用される視覚効果を設定します。現在のソフトウェアレンダラーでは
// 実装されていません（将来の拡張のためのプレースホルダー）。
void render_set_post_effect(render_post_effect_t post) {
	// TODO: 将来実装予定
}

// フレーム描画の準備
// -----------------------------------------------------------------------------
// 【初心者向け解説】
// 新しいフレームの描画を開始する前の準備を行います。
// 画面バッファを取得し、黒色（0）でクリアします。
void render_frame_prepare(void) {
	// プラットフォームから画面バッファを取得
	screen_buffer = platform_get_screenbuffer(&screen_pitch);
	
	// 1行あたりのピクセル数を計算
	screen_ppr = screen_pitch / sizeof(rgba_t);

	// 画面を黒色でクリア
	memset(screen_buffer, 0, screen_size.y * screen_pitch);
}

// フレーム描画の終了
// -----------------------------------------------------------------------------
// 【初心者向け解説】
// フレームの描画が完了した後の処理を行います。
// ソフトウェアレンダラーでは特に必要な処理はありません。
void render_frame_end(void) {
	// 特に何もする必要なし
}

// 四角形（クワッド）の描画
// -----------------------------------------------------------------------------
// 【初心者向け解説】
// テクスチャを指定された位置とサイズで画面に描画します。
// ソフトウェアレンダラーは簡易実装のため、回転や剪断（せんだん）変形には
// 対応していません。軸に平行な四角形のみを描画できます。
void render_draw_quad(quadverts_t *quad, texture_t texture_handle) {
	// テクスチャハンドルの有効性をチェック
	error_if(texture_handle.index >= textures_len, "Invalid texture %d", texture_handle.index);

	// 注意: この実装は軸に平行な四角形のみをサポート
	// 回転や剪断（変形）には対応していない

	// 頂点データとその色を取得
	vertex_t *v = quad->vertices;
	rgba_t color = v[0].color;  // 頂点の色（すべての頂点で同じと仮定）

	// 描画先（画面上）の座標とサイズ
	int dx = v[0].pos.x;  // 左上X座標
	int dy = v[0].pos.y;  // 左上Y座標
	int dw = v[2].pos.x - dx;  // 幅
	int dh = v[2].pos.y - dy;  // 高さ

	// テクスチャ情報の取得
	vec2i_t src_size = textures[texture_handle.index].size;  // テクスチャサイズ
	rgba_t *src_px = textures[texture_handle.index].pixels;  // テクスチャピクセルデータ

	// テクスチャ座標（UV）の左上と右下を計算して範囲内に制限
	vec2i_t uv_tl = vec2i_from_vec2(v[0].uv);  // 左上のUV座標
	uv_tl.x = clamp(uv_tl.x, 0, src_size.x);   // 範囲内に制限
	uv_tl.y = clamp(uv_tl.y, 0, src_size.y);   // 範囲内に制限

	vec2i_t uv_br = vec2i_from_vec2(v[2].uv);  // 右下のUV座標
	uv_br.x = clamp(uv_br.x, 0, src_size.x);   // 範囲内に制限
	uv_br.y = clamp(uv_br.y, 0, src_size.y);   // 範囲内に制限

	// テクスチャ内の使用範囲
	float sx = uv_tl.x;        // 開始X座標
	float sy = uv_tl.y;        // 開始Y座標
	float sw = uv_br.x - sx;   // 幅
	float sh = uv_br.y - sy;   // 高さ

	// テクスチャ座標の増分（拡大縮小率）
	float sx_inc = sw / dw;    // X方向の増分
	float sy_inc = sh / dh;    // Y方向の増分

	// 画面範囲外をクリッピング（画面内に収める）
	// -----------------------------------------------------------------------------
	// 【初心者向け解説】
	// 四角形が画面からはみ出している場合、はみ出した部分の描画をスキップします。
	// これにより、画面外のメモリにアクセスすることを防ぎます。
	
	// 左側が画面外の場合
	if (dx < 0) {
		sx += sx_inc * -dx;  // テクスチャ開始位置を調整
		dw += dx;           // 幅を減らす
		dx = 0;             // 開始X座標を画面の端に設定
	}
	// 右側が画面外の場合
	if (dx + dw >= screen_size.x) {
		dw = screen_size.x - dx;  // 幅を画面内に収まるよう調整
	}
	// 上側が画面外の場合
	if (dy < 0) {
		sy += sy_inc * -dy;  // テクスチャ開始位置を調整
		dh += dy;           // 高さを減らす
		dy = 0;             // 開始Y座標を画面の端に設定
	}
	// 下側が画面外の場合
	if (dy + dh >= screen_size.y) {
		dh = screen_size.y - dy;  // 高さを画面内に収まるよう調整
	}

	// 実際の描画処理
	// -----------------------------------------------------------------------------
	// 【初心者向け解説】
	// テクスチャの各ピクセルを取得して、画面バッファに書き込みます。
	// テクスチャが拡大/縮小される場合は、ニアレストネイバー法（最近傍補間）で
	// サンプリングします。

	// 注意: sx_incやsy_incが負の場合、ソースデータにアンダーフローの可能性あり
	int di = dy * screen_ppr + dx;  // 描画先インデックス（画面バッファ内の位置）
	
	for (int y = 0; y < dh; y++, di += screen_ppr - dw) {
		// 丸め誤差を避けるため、0.001ピクセル分のバッファを追加
		float si = floor(sy + y * sy_inc) * src_size.x + sx + 0.001;
		
		for (int x = 0; x < dw; x++, si += sx_inc, di++) {
			// テクスチャピクセルと頂点色を混合し、既存の画面ピクセルとブレンド
			screen_buffer[di] = rgba_blend(
				screen_buffer[di],     // 画面上の既存ピクセル
				rgba_mix(src_px[(int)si], color)  // テクスチャピクセルと頂点色を混合
			);
		}
	}
}

// 現在のテクスチャ状態をマークする
// -----------------------------------------------------------------------------
// 【初心者向け解説】
// 現在のテクスチャリストの状態を記録し、後で textures_reset で
// その状態に戻すことができるようにします。
// これは一時的なテクスチャを使用する場合に便利です。
texture_mark_t textures_mark(void) {
	return (texture_mark_t){.index = textures_len};  // 現在のテクスチャ数を記録
}

// テクスチャをマークした時点の状態にリセットする
// -----------------------------------------------------------------------------
// 【初心者向け解説】
// textures_mark で記録した状態にテクスチャリストを戻します。
// マーク以降に追加されたテクスチャは「忘れられ」、もう使用できなくなります。
// メモリリークを防ぐために、ゲームのレベル切り替えなどで使用します。
void textures_reset(texture_mark_t mark) {
	// マークが有効な範囲内かチェック
	error_if(mark.index > textures_len, "Invalid texture reset mark %d >= %d", mark.index, textures_len);
	
	// テクスチャ数をマークした時点に戻す
	textures_len = mark.index;
	
	// 注意: 実際のメモリ解放は行っていない（bump_allocを使用しているため）
	// 実際のアプリケーションでは、明示的にメモリを解放する必要があるかもしれない
}

// 新しいテクスチャを作成する
// -----------------------------------------------------------------------------
// 【初心者向け解説】
// ピクセルデータから新しいテクスチャを作成し、テクスチャリストに追加します。
// 画像ファイルをロードした後、その画像データをテクスチャとして登録するために
// 使用されます。
texture_t texture_create(vec2i_t size, rgba_t *pixels) {
	// テクスチャの最大数チェック
	error_if(textures_len >= RENDER_TEXTURES_MAX, "RENDER_TEXTURES_MAX reached");

	// テクスチャ情報の設定
	textures[textures_len].size = size;  // サイズを保存
	
	// ピクセルデータ用のメモリを確保してコピー
	textures[textures_len].pixels = bump_alloc(sizeof(rgba_t) * size.x * size.y);
	memcpy(textures[textures_len].pixels, pixels, sizeof(rgba_t) * size.x * size.y);

	// テクスチャハンドルを作成
	texture_t texture_handle = {.index = textures_len};
	textures_len++;  // テクスチャ数を増やす
	return texture_handle;
}

// 既存テクスチャのピクセルデータを更新する
// -----------------------------------------------------------------------------
// 【初心者向け解説】
// 既に作成済みのテクスチャの内容を新しいピクセルデータで置き換えます。
// アニメーションや動的に変化するテクスチャを実現するために使用されます。
void texture_replace_pixels(texture_t texture_handle, vec2i_t size, rgba_t *pixels) {
	// テクスチャハンドルの有効性をチェック
	error_if(texture_handle.index >= textures_len, "Invalid texture %d", texture_handle.index);

	// 対象テクスチャの情報を取得
	vec2i_t dst_size = textures[texture_handle.index].size;
	rgba_t *dst_px = textures[texture_handle.index].pixels;
	
	// 新しいデータが対象テクスチャに収まるかチェック
	error_if(dst_size.x < size.x || dst_size.y < size.y, 
		"Cannot replace %dx%d pixels of %dx%d texture", size.x, size.y, dst_size.x, dst_size.y);

	// ピクセルデータをコピー
	int di = 0;  // 宛先インデックス
	int si = 0;  // ソースインデックス
	for (int y = 0; y < size.y; y++, di += dst_size.x - size.x) {
		for (int x = 0; x < size.x; x++, si++, di++) {
			dst_px[di] = pixels[si];  // ピクセルをコピー
		}
	}
}
