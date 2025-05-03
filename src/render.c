#include <math.h>
#include "render.h"
#include "utils.h"

// レンダラー共通の実装
// =============================================================================
// 【初心者向け解説】
// このファイルは、レンダリングシステムの共通部分を実装しています。
// 異なるグラフィックスバックエンド（OpenGL、Metal、ソフトウェア）に依存しない
// 処理を担当します。
//
// レンダリングシステムは以下の層に分かれています：
// 1. 共通インターフェース（render.h）: すべてのレンダラーが実装するAPI
// 2. 共通実装（render.c）: このファイル。座標変換や一般的な処理
// 3. バックエンド固有実装: OpenGL/Metal/ソフトウェアの具体的な描画処理
//
// この構造により、ゲームコードはどのレンダラーを使用しているか意識せずに
// 描画命令を出すことができます。

// コンパイル時に指定されたレンダラーバックエンドを含める
// -----------------------------------------------------------------------------
// 【初心者向け解説】
// プリプロセッサ指示子（#if/#elif/#else）を使って、コンパイル時に指定された
// レンダラーの実装ファイルを含めます。
//
// 複数のレンダラーバックエンドが必要な理由：
// - OpenGL: 多くのプラットフォームで動作する標準的なAPI
// - Metal: Apple製品（Mac/iPhone）での高速な描画
// - ソフトウェア: グラフィックカードがない環境でも動作
//
// コンパイル時に「RENDER_GL」などのマクロを定義することで、使用するレンダラーを指定します。
#if defined(RENDER_GL)
	#include "render_gl.c"  // OpenGLレンダラー
#elif defined(RENDER_SOFTWARE)
	#include "render_software.c"  // ソフトウェアレンダラー
#elif defined(RENDER_METAL)
	#include "render_metal.m"  // Metalレンダラー（Apple製品用）
#else
	#error "No renderer specified. #define RENDER_GL or RENDER_SOFTWARE"
#endif


// グローバル変数（静的変数）
// -----------------------------------------------------------------------------
// 【初心者向け解説】
// これらの変数は、レンダリングシステムの状態を保持します。
// staticキーワードにより、このファイル内でのみアクセス可能になっています。

static uint32_t draw_calls = 0;       // 描画命令（ドローコール）の回数
static float screen_scale;            // 論理画面サイズから実際の画面サイズへの拡大率
static float inv_screen_scale;        // screen_scaleの逆数（計算を効率化するため）
static vec2i_t screen_size;           // 実際の画面サイズ（ピクセル単位）
static vec2i_t logical_size;          // 論理画面サイズ（ゲーム内の座標系）

// トランスフォーム（変換）スタック
// -----------------------------------------------------------------------------
// 【初心者向け解説】
// トランスフォームスタックは、描画時の位置、回転、拡大縮小を管理します。
// 行列（mat3_t）という数学的な道具を使って、これらの変換を効率的に処理します。
//
// 行列については深く理解する必要はありませんが、これにより複数の変換
// （移動→回転→拡大など）を効率的に組み合わせることができます。
static mat3_t transform_stack[RENDER_TRANSFORM_STACK_SIZE];  // 変換行列のスタック
static uint32_t transform_stack_index = 0;                   // 現在のスタック位置

// レンダラーの初期化
// -----------------------------------------------------------------------------
// 【初心者向け解説】
// レンダリングシステムの初期化を行います。以下の処理を実行します：
// 1. 選択されたバックエンド（OpenGL/Metal/ソフトウェア）の初期化
// 2. 画面サイズの設定
// 3. トランスフォームスタックの初期化
//
// アプリケーション起動時に一度だけ呼び出されます。
void render_init(vec2i_t avaiable_size) {
	render_backend_init();               // バックエンド固有の初期化
	render_resize(avaiable_size);        // 画面サイズの設定
	transform_stack[0] = mat3_identity(); // 単位行列（変換なし）で初期化
}

// レンダラーのクリーンアップ
// -----------------------------------------------------------------------------
// 【初心者向け解説】
// レンダリングシステムの終了処理を行います。確保したリソースを解放します。
// アプリケーション終了時に一度だけ呼び出されます。
void render_cleanup(void) {
	render_backend_cleanup(); // バックエンド固有のクリーンアップ
}

// 描画命令（ドローコール）回数の取得とリセット
// -----------------------------------------------------------------------------
// 【初心者向け解説】
// 前回のフレームでの描画命令の回数を返し、カウンターをリセットします。
// パフォーマンスの監視やデバッグに役立ちます。
//
// ドローコールとは、グラフィックスAPIに対する描画指示のことで、多いほど
// パフォーマンスが低下する可能性があります。
uint32_t render_draw_calls(void) {
	uint32_t r = draw_calls; // 現在の値を保存
	draw_calls = 0;          // カウンターをリセット
	return r;                // 保存した値を返す
}

// 画面のリサイズ処理
// -----------------------------------------------------------------------------
// 【初心者向け解説】
// ウィンドウサイズが変更されたときに、論理解像度（ゲーム内座標系）と
// 実際の解像度（画面ピクセル）の関係を再計算します。
//
// この関数は次の3つの主要な処理を行います：
// 1. 拡大率（ズーム）の計算
// 2. 実際の画面サイズの決定
// 3. 論理サイズの計算と設定の適用
void render_resize(vec2i_t avaiable_size) {
	// 拡大率（ズーム）の計算
	// スケールモードに応じて異なる計算を行う
	if (RENDER_SCALE_MODE == RENDER_SCALE_NONE) {
		screen_scale = 1;  // スケーリングなし（1:1のピクセルマッピング）
	}
	else {
		// 利用可能な画面サイズと論理サイズの比率から拡大率を計算
		// 幅と高さの両方がフィットする最大の拡大率を選択
		screen_scale = min(
			avaiable_size.x / (float)RENDER_WIDTH,
			avaiable_size.y / (float)RENDER_HEIGHT
		);

		// 離散的スケーリングの場合、整数値または0.5単位に丸める
		if (RENDER_SCALE_MODE == RENDER_SCALE_DISCRETE) {
			screen_scale = max(floor(screen_scale), 0.5);
		}
	}

	// 実際の画面サイズの決定
	// リサイズモードに応じて幅を計算
	if (RENDER_RESIZE_MODE & RENDER_RESIZE_WIDTH) {
		// 幅の拡大を許可する場合
		screen_size.x = max(avaiable_size.x, RENDER_WIDTH);
	}
	else {
		// 幅は固定拡大率
		screen_size.x = RENDER_WIDTH * screen_scale;
	}

	// リサイズモードに応じて高さを計算
	if (RENDER_RESIZE_MODE & RENDER_RESIZE_HEIGHT) {
		// 高さの拡大を許可する場合
		screen_size.y = max(avaiable_size.y, RENDER_HEIGHT);
	}
	else {
		// 高さは固定拡大率
		screen_size.y = RENDER_HEIGHT * screen_scale;
	}

	// 論理サイズの計算と設定の適用
	logical_size.x = ceil(screen_size.x / screen_scale);  // 論理幅
	logical_size.y = ceil(screen_size.y / screen_scale);  // 論理高さ
	inv_screen_scale = 1.0 / screen_scale;  // 逆拡大率（計算効率化のため）
	render_set_screen(screen_size);  // バックエンドに新しい画面サイズを設定
}

// 論理画面サイズを取得
// -----------------------------------------------------------------------------
// 【初心者向け解説】
// 現在の論理画面サイズ（ゲーム内で使用される座標系のサイズ）を返します。
// これはゲームロジックが「画面の大きさはどれくらいか」を知るために使用します。
vec2i_t render_size(void) {
	return logical_size;
}

// トランスフォームスタック操作: 新しい変換行列をプッシュ
// -----------------------------------------------------------------------------
// 【初心者向け解説】
// トランスフォームスタックに新しい変換行列をプッシュします。これにより、
// 現在の変換状態を保存して、新しい変換を始めることができます。
//
// 例えば、キャラクターを描画するとき、まずキャラクターの位置に移動し、
// その位置からキャラクターの各パーツを描画することができます。
// 後で元の位置に戻るために、render_pop()を呼び出します。
void render_push(void) {
	error_if(transform_stack_index >= RENDER_TRANSFORM_STACK_SIZE-1, "Max transform stack size (%d) reached", RENDER_TRANSFORM_STACK_SIZE);
	transform_stack[transform_stack_index+1] = transform_stack[transform_stack_index];
	transform_stack_index++;
}

// トランスフォームスタック操作: 変換行列をポップ
// -----------------------------------------------------------------------------
// 【初心者向け解説】
// トランスフォームスタックから最新の変換行列を取り除きます。
// これにより、前の変換状態に戻ることができます。
//
// render_push()とセットで使用され、「一時的な変換」を行うために必要です。
void render_pop(void) {
	error_if(transform_stack_index == 0, "Cannot pop from empty transform stack");
	transform_stack_index--;
}

// 現在の変換行列に平行移動（移動）を適用
// -----------------------------------------------------------------------------
// 【初心者向け解説】
// 現在の変換行列に平行移動（x,y方向への移動）を適用します。
// 移動量は論理座標系で指定し、内部で実際の画面座標に変換されます。
//
// これを使って、描画する位置を変更できます。
// 例: render_translate(vec2(100, 50)); // x方向に100、y方向に50移動
void render_translate(vec2_t translate) {
	error_if(transform_stack_index == 0, "Cannot translate initial transform. render_push() first.");
	translate = vec2_mulf(translate, screen_scale);  // 論理座標から実際の画面座標に変換
	mat3_translate(&transform_stack[transform_stack_index], translate);  // 変換行列に移動を適用
}

// 現在の変換行列に拡大縮小を適用
// -----------------------------------------------------------------------------
// 【初心者向け解説】
// 現在の変換行列に拡大縮小を適用します。1.0が等倍、2.0が2倍、0.5が半分のサイズです。
//
// x方向とy方向で異なる拡大率を設定できるため、形を伸縮させることもできます。
// 例: render_scale(vec2(2.0, 1.0)); // 横方向だけ2倍に拡大
void render_scale(vec2_t scale) {
	error_if(transform_stack_index == 0, "Cannot scale initial transform. render_push() first.");
	mat3_scale(&transform_stack[transform_stack_index], scale);  // 変換行列に拡大縮小を適用
}

// 現在の変換行列に回転を適用
// -----------------------------------------------------------------------------
// 【初心者向け解説】
// 現在の変換行列に回転を適用します。角度はラジアン単位で指定します。
// （2πラジアン = 360度、π/2ラジアン = 90度）
//
// 回転は現在の原点（移動や前の変換で決まる点）を中心に行われます。
// 例: render_rotate(3.14159 / 4); // 45度回転
void render_rotate(float rotation) {
	error_if(transform_stack_index == 0, "Cannot rotate initial transform. render_push() first.");
	mat3_rotate(&transform_stack[transform_stack_index], rotation);  // 変換行列に回転を適用
}

// 位置をピクセル単位にスナップ（吸着）
// -----------------------------------------------------------------------------
// 【初心者向け解説】
// 論理座標を実際のピクセル境界に吸着させ、再び論理座標に変換します。
// これは、ドット絵のようなピクセルアートを描画するときに、
// ピクセルが半端な位置にならないようにするために使用されます。
//
// 結果として、より鮮明でくっきりとした表示を得ることができます。
vec2_t render_snap_px(vec2_t pos) {
	vec2_t sp = vec2_mulf(pos, screen_scale);  // 論理座標から画面座標に変換
	return vec2_mulf(vec2(round(sp.x), round(sp.y)), inv_screen_scale);  // 四捨五入して論理座標に戻す
}

// テクスチャを描画する
// -----------------------------------------------------------------------------
// 【初心者向け解説】
// テクスチャ（画像）を指定された位置とサイズで描画します。これはゲーム内の
// スプライト（キャラクター、アイテム、背景など）を表示するための基本関数です。
//
// パラメータ:
// - pos: 描画位置（左上隅）の論理座標
// - size: 描画サイズ（幅と高さ）の論理単位
// - texture_handle: 描画するテクスチャのハンドル
// - uv_offset: テクスチャ内の使用する領域の開始位置（0～1の範囲）
// - uv_size: テクスチャ内の使用する領域のサイズ（0～1の範囲）
// - color: 乗算する色（白=等倍、他の色=色調変更、透明度も制御可能）
void render_draw(vec2_t pos, vec2_t size, texture_t texture_handle, vec2_t uv_offset, vec2_t uv_size, rgba_t color) {
	// 画面外にある場合は描画をスキップ（最適化）
	if (
		pos.x > logical_size.x || pos.y > logical_size.y ||
		pos.x + size.x < 0     || pos.y + size.y < 0
	) {
		return;
	}

	// 論理座標を画面座標に変換
	pos = vec2_mulf(pos, screen_scale);
	size = vec2_mulf(size, screen_scale);
	draw_calls++;  // 描画命令カウンタを増加

	// 四角形（クワッド）の頂点データを作成
	// 【初心者向け解説】
	// テクスチャは常に4つの頂点を持つ四角形（クワッド）として描画されます。
	// 各頂点には以下の情報が含まれます：
	// - pos: 画面上の位置（ピクセル単位）
	// - uv: テクスチャ内のどの部分を使用するか（0～1の範囲）
	// - color: 頂点の色（テクスチャの色に乗算される）
	quadverts_t q = {
		.vertices = {
			{
				.pos = {pos.x, pos.y},  // 左上
				.uv = {uv_offset.x , uv_offset.y},  // テクスチャの左上
				.color = color
			},
			{
				.pos = {pos.x + size.x, pos.y},  // 右上
				.uv = {uv_offset.x +  uv_size.x, uv_offset.y},  // テクスチャの右上
				.color = color
			},
			{
				.pos = {pos.x + size.x, pos.y + size.y},  // 右下
				.uv = {uv_offset.x + uv_size.x, uv_offset.y + uv_size.y},  // テクスチャの右下
				.color = color
			},
			{
				.pos = {pos.x, pos.y + size.y},  // 左下
				.uv = {uv_offset.x, uv_offset.y + uv_size.y},  // テクスチャの左下
				.color = color
			}
		}
	};

	// 変換行列（移動・回転・拡大縮小）を適用
	if (transform_stack_index > 0) {
		mat3_t *m = &transform_stack[transform_stack_index];
		for (uint32_t i = 0; i < 4; i++) {
			q.vertices[i].pos = vec2_transform(q.vertices[i].pos, m);  // 各頂点に変換を適用
		}
	}

	// 実際の描画をレンダラーバックエンドに依頼
	render_draw_quad(&q, texture_handle);
}
