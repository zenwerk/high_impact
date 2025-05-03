#include "engine.h"
#include "render.h"
#include "alloc.h"
#include "utils.h"

// OpenGLレンダラー
// =============================================================================
// 【初心者向け解説】
// このファイルはOpenGL APIを使用したレンダラーの実装です。OpenGLは広く使われている
// クロスプラットフォームのグラフィックスAPIで、多くのプラットフォーム（Windows、
// Linux、macOS、その他）で動作します。
//
// 他のレンダラー実装と比較：
// - OpenGL: クロスプラットフォーム、広くサポート、比較的使いやすい
// - Metal: AppleデバイスのみでのみBF動作、パフォーマンスが良い
// - ソフトウェア: グラフィックスカードがなくても動作するが遅い
//
// このファイルは「render.c」からインクルードされ、コンパイル時に
// RENDER_GLが定義されている場合にのみ使用されます。

// アトラステクスチャ設定
// -----------------------------------------------------------------------------
// 【初心者向け解説】
// アトラステクスチャとは、複数の小さなテクスチャを1つの大きなテクスチャに
// まとめたものです。これにより描画パフォーマンスが向上します。
// 
// ゲームでは多くの小さな画像（キャラクター、アイテムなど）を使いますが、
// 描画のたびにテクスチャを切り替えるのは非効率です。代わりに、すべての画像を
// 1つの大きなテクスチャ（アトラス）にまとめ、その一部を切り取って使用します。

#if !defined(RENDER_ATLAS_SIZE)
	#define RENDER_ATLAS_SIZE 64  // アトラスの分割グリッド数
#endif

#if !defined(RENDER_ATLAS_GRID)
	#define RENDER_ATLAS_GRID 32  // 各グリッドのピクセルサイズ
#endif

#if !defined(RENDER_ATLAS_BORDER)
	#define RENDER_ATLAS_BORDER 0  // テクスチャ間の境界ピクセル数
#endif

#define RENDER_ATLAS_SIZE_PX (RENDER_ATLAS_SIZE * RENDER_ATLAS_GRID)  // アトラス全体のピクセルサイズ

// バッファ容量と描画設定
// -----------------------------------------------------------------------------
#if !defined(RENDER_BUFFER_CAPACITY)
	#define RENDER_BUFFER_CAPACITY 2048  // 一度に描画できる四角形の最大数
#endif

#if !defined(RENDER_USE_MIPMAPS)
	#define RENDER_USE_MIPMAPS 0  // ミップマップの使用フラグ（テクスチャの縮小表示品質向上）
#endif


// OpenGLのプラットフォーム依存ロード
// -----------------------------------------------------------------------------
// 【初心者向け解説】
// OpenGLは各プラットフォーム（Windows、Mac、Linux、WebなどJでS少し異なる方法で
// ロードする必要があります。このセクションでは、現在のプラットフォームに応じて
// 適切なOpenGLヘッダーを選択しています。

#if defined(__EMSCRIPTEN__)
	// Emscripten（Web向け）: ローダー不要
	#include <GLES3/gl3.h>
#elif defined(__APPLE__) && defined(__MACH__)
	// macOS: ローダー不要
	#include <OpenGL/gl3.h>
#elif defined(__unix__)
	// Linux: 関数プロトタイプの明示的設定が必要
	#define GL_GLEXT_PROTOTYPES
	#include <GL/gl.h>
#else
	// Windows: gladライブラリを使用してOpenGLをロード
	#define RENDER_HAS_GLAD
	#include "../libs/glad.c"
#endif


// シェーダーコンパイル用マクロと関数
// -----------------------------------------------------------------------------
// 【初心者向け解説】
// シェーダーとは、GPUで実行される小さなプログラムで、頂点の位置計算や
// ピクセルの色の計算を行います。これらは「GLSL」と呼ばれる言語で書かれています。
//
// OpenGLでは、頂点シェーダー（頂点の位置を計算）と
// フラグメントシェーダー（ピクセルの色を計算）の2種類が基本です。

// 現在のシェーダープログラムを使用するマクロ
#define use_program(SHADER) \
	glUseProgram((SHADER)->program); \
	glBindVertexArray((SHADER)->vao);

// 浮動小数点頂点属性をバインドするマクロ
#define bind_va_f(index, container, member, start) \
	glVertexAttribPointer( \
		index, member_size(container, member)/sizeof(float), GL_FLOAT, false, \
		sizeof(container), \
		(GLvoid*)(offsetof(container, member) + start) \
	)

// 色属性をバインドするマクロ
#define bind_va_color(index, container, member, start) \
	glVertexAttribPointer( \
		index, 4,  GL_UNSIGNED_BYTE, true, \
		sizeof(container), \
		(GLvoid*)(offsetof(container, member) + start) \
	)

// シェーダーをコンパイルする関数
// -----------------------------------------------------------------------------
// 【初心者向け解説】
// この関数は、シェーダーのソースコードを受け取り、コンパイルして
// 使用可能なシェーダーオブジェクトを返します。
static GLuint compile_shader(GLenum type, const char *source) {
	GLuint shader = glCreateShader(type);         // 新しいシェーダーオブジェクトを作成
	glShaderSource(shader, 1, &source, NULL);    // ソースコードを設定
	glCompileShader(shader);                     // シェーダーをコンパイル
	
	// コンパイルエラーチェック
	GLint success;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
	if (!success) {
		int log_written;
		char log[256];
		glGetShaderInfoLog(shader, 256, &log_written, log);
		die("Error compiling shader: %s\nwith source:\n%s", log, source);
	}
	return shader;
}

// シェーダープログラムを作成する関数
// -----------------------------------------------------------------------------
// 【初心者向け解説】
// この関数は、頂点シェーダーとフラグメントシェーダーのソースコードを受け取り、
// それらをコンパイルしてリンクし、完全なシェーダープログラムを作成します。
//
// シェーダープログラムは、描画時にGPUで実行される一連の命令です。
static GLuint create_program(const char *vs_source, const char *fs_source) {
	GLuint vs = compile_shader(GL_VERTEX_SHADER, vs_source);    // 頂点シェーダーをコンパイル
	GLuint fs = compile_shader(GL_FRAGMENT_SHADER, fs_source);  // フラグメントシェーダーをコンパイル

	GLuint program = glCreateProgram();           // 新しいプログラムオブジェクトを作成
	glAttachShader(program, vs);                 // 頂点シェーダーを添付
	glAttachShader(program, fs);                 // フラグメントシェーダーを添付
	glLinkProgram(program);                      // プログラムをリンク
	glUseProgram(program);                       // プログラムを使用
	return program;
}

// シェーダー言語とバージョン
// -----------------------------------------------------------------------------
// 【初心者向け解説】
// OpenGLのシェーダー言語（GLSL）は、異なるOpenGLのバージョンやプラットフォームで
// 微妙に構文が異なります。このコードは、同じシェーダーロジックを異なる環境で
// 再利用できるようにするためのものです。
//
// プリアンブル（前置き）を使って、各環境に合わせた適切なGLSLコードを生成します。

#if !defined(RENDER_GLSL_VERSION)
	#if defined(__EMSCRIPTEN__) || defined(USE_GLES2)
		// WebGL/OpenGL ES 2.0向け（モバイルやWeb）
		#define RENDER_SHADER_PREAMBLE_VS \
			"precision highp float;\n" \
			"#define IN attribute\n" \
			"#define OUT varying\n"
		#define RENDER_SHADER_PREAMBLE_FS \
			"precision highp float;\n" \
			"#define IN varying\n" \
			"#define FRAG_COLOR gl_FragColor\n" \
			"#define OUT_FRAG_COLOR\n"  \
			"#define TEXTURE texture2D\n"
	#else
		// デスクトップOpenGL 3.x+向け
		#define RENDER_SHADER_PREAMBLE_VS \
			"#version 140\n" \
			"#define IN in\n" \
			"#define OUT out\n"
		#define RENDER_SHADER_PREAMBLE_FS \
			"#version 140\n" \
			"#define IN in\n" \
			"#define FRAG_COLOR fragment_color_output\n" \
			"#define TEXTURE texture\n" \
			"out vec4 FRAG_COLOR;\n"
	#endif
#endif

// シェーダーソースコードを生成するマクロ
#define SHADER_SOURCE_VS(...) RENDER_SHADER_PREAMBLE_VS #__VA_ARGS__	
#define SHADER_SOURCE_FS(...) RENDER_SHADER_PREAMBLE_FS #__VA_ARGS__	


// ゲーム用メインシェーダー
// -----------------------------------------------------------------------------
// 【初心者向け解説】
// ゲームの通常描画に使用される2つのシェーダー：
// 1. 頂点シェーダー（SHADER_GAME_VS）：頂点の位置計算
// 2. フラグメントシェーダー（SHADER_GAME_FS）：ピクセルの色計算
//
// これらのシェーダーは、テクスチャを画面に描画する基本的な機能を提供します。

// ゲーム用頂点シェーダー - 頂点の位置と属性を処理
static const char * const SHADER_GAME_VS = SHADER_SOURCE_VS(
	IN vec2 pos;       // 頂点の位置
	IN vec2 uv;        // テクスチャ座標
	IN vec4 color;     // 頂点の色
	OUT vec4 v_color;  // フラグメントシェーダーに渡す色
	OUT vec2 v_uv;     // フラグメントシェーダーに渡すテクスチャ座標

	uniform vec2 screen;  // 画面サイズ
	uniform vec2 fade;    // フェード効果用
	uniform float time;   // 経過時間
	
	void main(void) {
		v_color = color;  // 色をそのまま出力
		v_uv = uv;        // テクスチャ座標をそのまま出力
		gl_Position = vec4(
			floor(pos + 0.5) * (vec2(2,-2)/screen.xy) + vec2(-1.0,1.0),  // 画面座標をクリップ空間に変換
			0.0, 1.0
		);
	}
);

// ゲーム用フラグメントシェーダー - ピクセルの色を計算
static const char * const SHADER_GAME_FS = SHADER_SOURCE_FS(
	IN vec4 v_color;  // 頂点シェーダーから受け取った色
	IN vec2 v_uv;     // 頂点シェーダーから受け取ったテクスチャ座標

	uniform sampler2D atlas;  // テクスチャアトラス

	void main(void) {
		vec4 tex_color = TEXTURE(atlas, v_uv);  // テクスチャから色を取得
		vec4 color = tex_color * v_color;       // テクスチャの色と頂点の色を乗算
		FRAG_COLOR = color;                     // 最終的なピクセルの色
	}
);

// ゲーム用シェーダープログラム構造体
// -----------------------------------------------------------------------------
// 【初心者向け解説】
// シェーダープログラムに関連するすべてのデータを保持するための構造体です。
// これには、プログラムID、頂点配列オブジェクト、ユニフォーム（定数値）の
// 位置、頂点属性の位置などが含まれます。
typedef struct {
	GLuint program;  // シェーダープログラムのID
	GLuint vao;      // 頂点配列オブジェクト
	struct {
		GLuint screen;  // 画面サイズユニフォームの位置
		GLuint time;    // 時間ユニフォームの位置
	} uniform;
	struct {
		GLuint pos;    // 位置属性の位置
		GLuint uv;     // テクスチャ座標属性の位置
		GLuint color;  // 色属性の位置
	} attribute;
} prg_game_t;

// ゲーム用シェーダープログラムの初期化
// -----------------------------------------------------------------------------
// 【初心者向け解説】
// この関数は、ゲーム描画用のシェーダープログラムを初期化します。
// シェーダーをコンパイル・リンクし、必要な属性とユニフォームの位置を取得し、
// 頂点データの形式を設定します。
prg_game_t *shader_game_init(void) {
	prg_game_t *s = bump_alloc(sizeof(prg_game_t));  // メモリ割り当て
	
	// シェーダープログラムの作成
	s->program = create_program(SHADER_GAME_VS, SHADER_GAME_FS);
	
	// ユニフォーム（シェーダーに渡す定数値）の位置を取得
	s->uniform.screen = glGetUniformLocation(s->program, "screen");

	// 頂点属性の位置を取得
	s->attribute.pos = glGetAttribLocation(s->program, "pos");
	s->attribute.uv = glGetAttribLocation(s->program, "uv");
	s->attribute.color = glGetAttribLocation(s->program, "color");

	// 頂点配列オブジェクト（VAO）の作成
	// VAOは頂点データの形式とバッファの関連付けを保存するオブジェクト
	glGenVertexArrays(1, &s->vao);
	glBindVertexArray(s->vao);

	// 使用する頂点属性を有効化
	glEnableVertexAttribArray(s->attribute.pos);
	glEnableVertexAttribArray(s->attribute.uv);
	glEnableVertexAttribArray(s->attribute.color);

	// 頂点データの形式を指定
	bind_va_f(s->attribute.pos, vertex_t, pos, 0);        // 位置（float型ベクトル）
	bind_va_f(s->attribute.uv, vertex_t, uv, 0);          // テクスチャ座標（float型ベクトル）
	bind_va_color(s->attribute.color, vertex_t, color, 0); // 色（バイト型RGBA）

	return s;  // 初期化されたシェーダープログラム構造体を返す
}


// ポストエフェクトシェーダー
// -----------------------------------------------------------------------------
// 【初心者向け解説】
// ポストエフェクトとは、すべての3D描画が完了した後に、画面全体に適用される
// 視覚効果のことです。例えば、古いテレビのようなブラウン管効果や、
// ぼかし、色調補正などがあります。
//
// このエンジンでは、通常の描画をいったん別のテクスチャに行い、
// その後そのテクスチャに効果を適用して画面に表示します。

// ポストエフェクト用頂点シェーダー
static const char * const SHADER_POST_VS = SHADER_SOURCE_VS(
	IN vec2 pos;       // 頂点の位置
	IN vec2 uv;        // テクスチャ座標
	OUT vec2 v_uv;     // フラグメントシェーダーに渡すテクスチャ座標

	uniform vec2 screen;  // 画面サイズ
	uniform float time;   // 経過時間
	
	void main(void) {
		gl_Position = vec4(
			pos * (vec2(2,-2)/screen.xy) + vec2(-1.0,1.0),  // 画面座標をクリップ空間に変換
			0.0, 1.0
		);
		v_uv = uv;  // テクスチャ座標をそのまま出力
	}
);

// 標準（効果なし）ポストエフェクトシェーダー
static const char * const SHADER_POST_FS_DEFAULT = SHADER_SOURCE_FS(
	IN vec2 v_uv;  // 頂点シェーダーから受け取ったテクスチャ座標

	uniform sampler2D screenbuffer;  // 画面内容のテクスチャ

	void main(void) {
		// テクスチャからそのままピクセルの色を読み取る（効果なし）
		FRAG_COLOR = TEXTURE(screenbuffer, v_uv);
	}
);

// ブラウン管（CRT）効果シェーダー
// -----------------------------------------------------------------------------
// 【初心者向け解説】
// 古いブラウン管テレビやモニタの見た目を再現するエフェクトです。
// 湾曲した画面、走査線、色のにじみなどの特徴があります。
//
// 元のコード: https://www.shadertoy.com/view/Ms23DR 
// 作者: https://github.com/mattiasgustavsson/
static const char * const SHADER_POST_FS_CRT = SHADER_SOURCE_FS(
	IN vec2 v_uv;  // テクスチャ座標

	uniform float time;             // 経過時間（アニメーション用）
	uniform sampler2D screenbuffer;  // 画面内容のテクスチャ
	uniform vec2 screen;            // 画面サイズ

	// テクスチャ座標を湾曲させる関数（古いテレビの曲面を再現）
	vec2 curve(vec2 uv) {
		uv = (uv - 0.5) * 2.0;  // 中心を原点に移動
		uv *= 1.1;	            // 少し拡大
		// x,y方向に湾曲を適用
		uv.x *= 1.0 + pow((abs(uv.y) / 5.0), 2.0);
		uv.y *= 1.0 + pow((abs(uv.x) / 4.0), 2.0);
		uv  = (uv / 2.0) + 0.5;  // 元の座標範囲に戻す
		uv =  uv *0.92 + 0.04;   // スケールと位置調整
		return uv;
	}

	void main(){
		// 座標を湾曲させる
		vec2 uv = curve(v_uv);
		vec3 color;
		
		// 時間とy座標に基づく揺らぎを計算（ノイズや揺れ効果）
		float x = 
			sin(0.3 * time + uv.y * 21.0) * sin(0.7 * time + uv.y * 29.0) *
			sin(0.3 + 0.33 * time + uv.y * 31.0) * 0.0017;

		// RGB各チャンネルを少しずらして取得（色収差効果）
		color.r = TEXTURE(screenbuffer, vec2(x + uv.x + 0.001, uv.y + 0.001)).x + 0.05;
		color.g = TEXTURE(screenbuffer, vec2(x + uv.x + 0.000, uv.y - 0.002)).y + 0.05;
		color.b = TEXTURE(screenbuffer, vec2(x + uv.x - 0.002, uv.y + 0.000)).z + 0.05;
		
		// ゴースト効果（2重映り）を追加
		color.r += 0.08 * TEXTURE(screenbuffer, 0.75 * vec2(x + 0.025, -0.027) + vec2(uv.x + 0.001, uv.y + 0.001)).x;
		color.g += 0.05 * TEXTURE(screenbuffer, 0.75 * vec2(x - 0.022, -0.020) + vec2(uv.x + 0.000, uv.y - 0.002)).y;
		color.b += 0.08 * TEXTURE(screenbuffer, 0.75 * vec2(x + -0.02, -0.018) + vec2(uv.x - 0.002, uv.y + 0.000)).z;

		// コントラスト調整
		color = clamp(color * 0.6 + 0.4 * color * color * 1.0, 0.0, 1.0);

		// ビネット効果（画面端が暗くなる）
		float vignette = (0.0 + 1.0 * 16.0 * uv.x * uv.y * (1.0 - uv.x) * (1.0 - uv.y));
		color *= vec3(pow(vignette, 0.25));
		
		// 色調整と明るさ
		color *= vec3(0.95,1.05,0.95);
		color *= 2.8;

		// 走査線効果（水平な暗い線）
		float scanlines = clamp( 0.35 + 0.35 * sin(3.5 * time + uv.y * screen.y * 1.5), 0.0, 1.0);
		float s = pow(scanlines,1.7);
		color = color * vec3(0.4 + 0.7 * s);

		// 時間に基づく揺らぎ
		color *= 1.0 + 0.01 * sin(110.0 * time);
		
		// 画面外の場合は黒にする
		if (uv.x < 0.0 || uv.x > 1.0) {
			color *= 0.0;
		}
		if (uv.y < 0.0 || uv.y > 1.0) {
			color *= 0.0;
		}
		
		// 交互ピクセル暗効果（ドット粒度感）
		color *= 1.0 - 0.65 * vec3(clamp((mod(gl_FragCoord.x, 2.0) - 1.0) * 2.0, 0.0, 1.0));
		
		// 最終的な色を出力（アルファ値は常に1.0）
		FRAG_COLOR = vec4(color, 1.0);
	}
);

// ポストエフェクト用シェーダープログラム構造体
// -----------------------------------------------------------------------------
// 【初心者向け解説】
// ポストエフェクト用のシェーダープログラムに関連するデータを保持する構造体です。
// ゲーム描画用のシェーダー（prg_game_t）と似ていますが、用途が異なります。
typedef struct {
	GLuint program;  // シェーダープログラムのID
	GLuint vao;      // 頂点配列オブジェクト
	struct {
		GLuint screen;  // 画面サイズユニフォームの位置
		GLuint time;    // 時間ユニフォームの位置
	} uniform;
	struct {
		GLuint pos;     // 位置属性の位置
		GLuint uv;      // テクスチャ座標属性の位置
	} attribute;
} prg_post_t;

// ポストエフェクトシェーダー共通の初期化処理
// -----------------------------------------------------------------------------
// 【初心者向け解説】
// すべてのポストエフェクトシェーダーに共通する初期化処理を行います。
// ユニフォーム（定数値）の位置を取得し、頂点属性を設定します。
void shader_post_general_init(prg_post_t *s) {
	// ユニフォーム（シェーダーに渡す定数値）の位置を取得
	s->uniform.screen = glGetUniformLocation(s->program, "screen");
	s->uniform.time = glGetUniformLocation(s->program, "time");

	// 頂点属性の位置を取得
	s->attribute.pos = glGetAttribLocation(s->program, "pos");
	s->attribute.uv = glGetAttribLocation(s->program, "uv");

	// 頂点配列オブジェクト（VAO）の作成と設定
	glGenVertexArrays(1, &s->vao);
	glBindVertexArray(s->vao);

	// 使用する頂点属性を有効化
	glEnableVertexAttribArray(s->attribute.pos);
	glEnableVertexAttribArray(s->attribute.uv);

	// 頂点データの形式を指定
	bind_va_f(s->attribute.pos, vertex_t, pos, 0);  // 位置（float型ベクトル）
	bind_va_f(s->attribute.uv, vertex_t, uv, 0);    // テクスチャ座標（float型ベクトル）
}

// 標準（エフェクトなし）ポストエフェクトシェーダーの初期化
// -----------------------------------------------------------------------------
// 【初心者向け解説】
// エフェクトを適用せずに画面をそのまま表示するシェーダーを初期化します。
prg_post_t *shader_post_default_init(void) {
	prg_post_t *s = bump_alloc(sizeof(prg_post_t));  // メモリ割り当て
	s->program = create_program(SHADER_POST_VS, SHADER_POST_FS_DEFAULT);  // シェーダープログラム作成
	shader_post_general_init(s);  // 共通初期化処理
	return s;
}

// CRT（ブラウン管）ポストエフェクトシェーダーの初期化
// -----------------------------------------------------------------------------
// 【初心者向け解説】
// 古いブラウン管テレビのような見た目にする効果シェーダーを初期化します。
// 湾曲、走査線、色にじみなどの効果が含まれます。
prg_post_t *shader_post_crt_init(void) {
	prg_post_t *s = bump_alloc(sizeof(prg_post_t));  // メモリ割り当て
	s->program = create_program(SHADER_POST_VS, SHADER_POST_FS_CRT);  // シェーダープログラム作成	
	shader_post_general_init(s);  // 共通初期化処理
	return s;
}



// レンダリング関連データと変数
// -----------------------------------------------------------------------------
// 【初心者向け解説】
// このセクションでは、レンダリングに必要なさまざまなデータ構造と変数を定義します。
// テクスチャ管理、頂点バッファ、画面サイズ情報などが含まれます。

// テクスチャアトラス内の位置情報
typedef struct {
	vec2i_t offset;  // アトラス内のピクセル位置
	vec2i_t size;    // テクスチャのサイズ（ピクセル単位）
} atlas_pos_t;

// 特殊なテクスチャハンドル（白い1x1テクスチャ、テクスチャなしを表す）
texture_t RENDER_NO_TEXTURE;

// 頂点データバッファ
static GLuint vbo_quads;    // 四角形（頂点）データのバッファオブジェクト
static GLuint vbo_indices;  // インデックス（頂点の接続情報）バッファオブジェクト

// バッファデータ（CPUメモリ上）
static quadverts_t quad_buffer[RENDER_BUFFER_CAPACITY];            // 頂点データの一時保存用
static uint16_t index_buffer[RENDER_BUFFER_CAPACITY][6];           // インデックスデータ（三角形2つで四角形）
static uint32_t quad_buffer_len = 0;                              // 現在のバッファ内の四角形数

// 画面サイズ情報
static vec2i_t screen_size;       // 実際の画面サイズ（ピクセル単位）
static vec2i_t backbuffer_size;   // バックバッファ（描画先）のサイズ

// テクスチャアトラス管理
static uint32_t atlas_map[RENDER_ATLAS_SIZE] = {0};    // アトラス内の空き領域管理
static GLuint atlas_texture = 0;                       // アトラステクスチャのGLハンドル
static render_blend_mode_t blend_mode = RENDER_BLEND_NORMAL;  // 現在のブレンドモード

// テクスチャ管理
static atlas_pos_t textures[RENDER_TEXTURES_MAX];      // 各テクスチャの位置情報
static uint32_t textures_len = 0;                      // 登録済みテクスチャ数
static bool mipmap_is_dirty = false;                   // ミップマップ再生成フラグ

// バックバッファ（画面外に描画するための一時的なテクスチャ）
static GLuint backbuffer = 0;           // バックバッファのフレームバッファオブジェクト
static GLuint backbuffer_texture = 0;   // バックバッファのテクスチャオブジェクト

// シェーダープログラム
prg_game_t *prg_game;  // ゲーム描画用シェーダー
prg_post_t *prg_post;  // 現在のポストエフェクトシェーダー
prg_post_t *prg_post_effects[RENDER_POST_MAX] = {};  // 利用可能なポストエフェクト一覧

// バッファのフラッシュ（実際のGPU描画）を行う関数の前方宣言
static void render_flush(void);


// static void gl_message_callback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei len, const GLchar *message, const void *userParam) {
// 	printf("GL: %s\n", message);
// }

// OpenGLレンダラーバックエンドの初期化
// -----------------------------------------------------------------------------
// 【初心者向け解説】
// この関数は、OpenGLレンダラーの初期化を行います。アプリケーション起動時に
// 一度だけ呼び出され、描画に必要なすべてのリソースを準備します。
//
// 主な初期化内容：
// 1. テクスチャアトラスの作成
// 2. 頂点バッファとインデックスバッファの準備
// 3. ポストエフェクトシェーダーの初期化
// 4. ゲーム描画用シェーダーの初期化
// 5. 基本的なOpenGL設定
void render_backend_init(void) {
	// Windows環境の場合は、OpenGL関数をロード
	#if defined(RENDER_HAS_GLAD)
		gladLoadGL();
	#endif

	// デバッグ出力（コメントアウト中）
	// glEnable(GL_DEBUG_OUTPUT);
	// glDebugMessageCallback(gl_message_callback, NULL);
	// glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, NULL, GL_TRUE);


	// テクスチャアトラスの作成
	// -------------------------
	// 【初心者向け解説】
	// テクスチャアトラスは、多くの小さなテクスチャを格納する1つの大きなテクスチャです。
	// 2048x2048などのサイズを持ち、ゲーム内のすべての画像を効率的に保存します。

	glGenTextures(1, &atlas_texture);  // テクスチャオブジェクトを生成
	glBindTexture(GL_TEXTURE_2D, atlas_texture);  // テクスチャをバインド（操作対象に設定）
	
	// テクスチャのフィルタリング設定
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);  // 拡大時はピクセル化（ドット絵向け）
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, RENDER_USE_MIPMAPS ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);  // 縮小時は滑らか
	
	// テクスチャの繰り返し設定（端で繰り返さず、端の色で塗りつぶす）
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	// テクスチャサイズとメモリ確保（内容はまだ空）
	uint32_t tw = RENDER_ATLAS_SIZE * RENDER_ATLAS_GRID;  // 幅
	uint32_t th = RENDER_ATLAS_SIZE * RENDER_ATLAS_GRID;  // 高さ
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tw, th, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	

	// 四角形頂点バッファの作成
	// -------------------------
	// 【初心者向け解説】
	// 頂点バッファは、GPUに送信する頂点データ（位置、色、テクスチャ座標など）
	// を保存するためのメモリ領域です。

	glGenBuffers(1, &vbo_quads);  // 頂点バッファオブジェクトを生成
	glBindBuffer(GL_ARRAY_BUFFER, vbo_quads);  // バッファをバインド

	// インデックスバッファの作成
	// -------------------------
	// 【初心者向け解説】
	// インデックスバッファは、頂点の接続方法を指定するデータです。
	// 四角形は2つの三角形から構成され、各三角形は3つの頂点インデックスで定義されます。

	glGenBuffers(1, &vbo_indices);  // インデックスバッファオブジェクトを生成
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo_indices);  // バッファをバインド

	// すべての四角形のインデックスデータを準備
	// 各四角形は4つの頂点と6つのインデックス（2つの三角形）を持つ
	for (uint32_t i = 0, j = 0; i < RENDER_BUFFER_CAPACITY; i++, j += 4) {
		// 最初の三角形: 左下→右上→左上
		index_buffer[i][0] = j + 3;  // 左下
		index_buffer[i][1] = j + 1;  // 右上
		index_buffer[i][2] = j + 0;  // 左上
		// 2番目の三角形: 左下→右下→右上
		index_buffer[i][3] = j + 3;  // 左下
		index_buffer[i][4] = j + 2;  // 右下
		index_buffer[i][5] = j + 1;  // 右上
	}
	// インデックスデータをGPUに送信
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(index_buffer), index_buffer, GL_STATIC_DRAW);


	// ポストエフェクトシェーダーの初期化
	// -------------------------
	// 【初心者向け解説】
	// ポストエフェクトは画面全体に適用される視覚効果です。
	// 各エフェクトのシェーダープログラムを初期化し、配列に格納します。

	prg_post_effects[RENDER_POST_NONE] = shader_post_default_init();  // エフェクトなし（標準）
	prg_post_effects[RENDER_POST_CRT] = shader_post_crt_init();       // ブラウン管エフェクト
	render_set_post_effect(RENDER_POST_NONE);  // 初期状態ではエフェクトなし

	// ゲーム描画シェーダーの初期化と基本設定
	// -------------------------
	// 【初心者向け解説】
	// ゲーム内の通常描画（スプライト、テキストなど）に使用するシェーダーを初期化し、
	// 基本的なOpenGL設定を行います。

	prg_game = shader_game_init();  // ゲーム描画用シェーダー初期化
	use_program(prg_game);          // シェーダーを使用状態に設定
	
	// 基本的なレンダリング設定
	glEnable(GL_CULL_FACE);         // 裏面カリング（見えない面を描画しない）
	glEnable(GL_BLEND);             // アルファブレンド（透明度対応）
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);  // 通常のアルファブレンド式

	// 「テクスチャなし」用の白テクスチャを作成
	// -------------------------
	// 【初心者向け解説】
	// テクスチャなしでも描画できるよう、純白の2x2テクスチャを作成します。
	// これにより、単色の四角形などを簡単に描画できます。

	rgba_t white_pixels[4] = {rgba_white(), rgba_white(), rgba_white(), rgba_white()};
	RENDER_NO_TEXTURE = texture_create(vec2i(2, 2), white_pixels);
}

void render_backend_cleanup(void) {
	// TODO
}

// 画面サイズの設定とバックバッファの初期化
// -----------------------------------------------------------------------------
// 【初心者向け解説】
// この関数は、画面サイズが変更されたときに呼び出され、描画先のバックバッファを
// 新しいサイズに合わせて再設定します。
//
// バックバッファとは、画面に直接描画するのではなく、一度別のメモリ（テクスチャ）に
// 描画してから、最終的に画面に表示するための一時的な描画領域です。
void render_set_screen(vec2i_t size) {
	screen_size = size;  // 画面サイズを保存
	backbuffer_size = screen_size;  // バックバッファも同じサイズに設定

	// 初回はバックバッファを作成
	if (!backbuffer) {
		glGenTextures(1, &backbuffer_texture);  // バックバッファのテクスチャを生成
		glGenFramebuffers(1, &backbuffer);      // フレームバッファオブジェクトを生成
	}
	
	// バックバッファテクスチャの設定
	glBindTexture(GL_TEXTURE_2D, backbuffer_texture);  // テクスチャをバインド
	// テクスチャの内容と形式を設定（RGB形式、指定サイズ）
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, backbuffer_size.x, backbuffer_size.y, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
	
	// フィルタリング設定（ピクセル化表示）
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	
	// テクスチャの繰り返し設定（端で繰り返さない）
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	
	// フレームバッファにテクスチャを接続
	glBindFramebuffer(GL_FRAMEBUFFER, backbuffer);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, backbuffer_texture, 0);

	// アトラステクスチャのフィルタリング設定を更新
	glBindTexture(GL_TEXTURE_2D, atlas_texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, RENDER_USE_MIPMAPS ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
	
	// ビューポート（描画領域）をバックバッファサイズに設定
	glViewport(0, 0, backbuffer_size.x, backbuffer_size.y);
}

// ポストエフェクトの設定
// -----------------------------------------------------------------------------
// 【初心者向け解説】
// この関数は、画面に適用するポストエフェクト（視覚効果）を変更します。
// 例えば、CRTエフェクト（古いテレビ風）や、エフェクトなしの通常表示などを切り替えます。
void render_set_post_effect(render_post_effect_t post) {
	// 不正なエフェクト番号のチェック
	error_if(post < 0 || post > RENDER_POST_MAX, "Invalid post effect %d", post);
	// 指定されたエフェクトを現在のポストエフェクトとして設定
	prg_post = prg_post_effects[post];
}

// フレーム描画の準備
// -----------------------------------------------------------------------------
// 【初心者向け解説】
// 各フレーム（画面更新）の開始時に呼び出される関数です。
// バックバッファをクリアし、描画の準備を整えます。
//
// この後、ゲームコードが様々な描画命令を発行します。
void render_frame_prepare(void) {
	// ゲーム描画用シェーダープログラムを有効化
	use_program(prg_game);
	
	// バックバッファを描画先に設定
	glBindFramebuffer(GL_FRAMEBUFFER, backbuffer);
	glViewport(0, 0, backbuffer_size.x, backbuffer_size.y);

	// テクスチャアトラスをバインド
	glBindTexture(GL_TEXTURE_2D, atlas_texture);
	
	// シェーダーに画面サイズを伝える
	glUniform2f(prg_game->uniform.screen, backbuffer_size.x, backbuffer_size.y);
	
	// バックバッファを黒色でクリア
	glClearColor(0, 0, 0, 1);  // 黒色（RGBA: 0,0,0,1）
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);  // 色バッファと深度バッファをクリア
	
	// 2Dゲームなので深度テストは無効化
	glDisable(GL_DEPTH_TEST); 
}

// フレーム描画の終了
// -----------------------------------------------------------------------------
// 【初心者向け解説】
// 各フレーム（画面更新）の最後に呼び出される関数です。
// バックバッファに描画された内容を実際の画面に表示します。
// このときにポストエフェクト（視覚効果）も適用されます。
void render_frame_end(void) {
	// 保留中の描画をすべて実行
	render_flush();

	// バックバッファを画面に描画
	// -------------------------
	
	// ポストエフェクトシェーダーを有効化
	use_program(prg_post);

	// 描画先を実際の画面に変更
	glBindFramebuffer(GL_FRAMEBUFFER, 0);  // フレームバッファ0は画面を表す
	glViewport(0, 0, screen_size.x, screen_size.y);  // 画面サイズに合わせる
	
	// バックバッファテクスチャをシェーダーの入力として設定
	glBindTexture(GL_TEXTURE_2D, backbuffer_texture);
	
	// シェーダーに時間と画面サイズを伝える（エフェクトのアニメーションに使用）
	glUniform1f(prg_post->uniform.time, engine.time);
	glUniform2f(prg_post->uniform.screen, screen_size.x, screen_size.y);

	// 画面を黒色でクリア
	glClearColor(0, 0, 0, 1);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// 画面全体を覆う四角形（バックバッファの内容を表示するキャンバス）
	quad_buffer[quad_buffer_len] = (quadverts_t){
		.vertices = {
			// 左上
			{.pos = {0,             0            }, .uv = {0, 1}, .color = rgba_white()},
			// 右上
			{.pos = {screen_size.x, 0            }, .uv = {1, 1}, .color = rgba_white()},
			// 右下
			{.pos = {screen_size.x, screen_size.y}, .uv = {1, 0}, .color = rgba_white()},
			// 左下
			{.pos = {0,             screen_size.y}, .uv = {0, 0}, .color = rgba_white()},
		}
	};
	quad_buffer_len++;  // 描画する四角形の数を増やす
	
	// 画面に描画を実行
	render_flush();
}

// 描画バッファのフラッシュ（GPUへの送信と描画実行）
// -----------------------------------------------------------------------------
// 【初心者向け解説】
// この関数は、これまでに蓄積した描画命令（四角形のリスト）をGPUに送信して
// 実際に描画を実行します。パフォーマンスのため、描画命令は一度にまとめて
// GPUに送ることで効率的に処理されます。
void render_flush(void) {
	// ミップマップが必要な場合は生成
	if (mipmap_is_dirty) {
		glGenerateMipmap(GL_TEXTURE_2D);  // テクスチャの縮小表示用の最適化データを生成
		mipmap_is_dirty = false;
	}

	// 描画する四角形がない場合は何もしない
	if (quad_buffer_len == 0) {
		return;
	}

	// 頂点データをGPUに送信
	glBindBuffer(GL_ARRAY_BUFFER, vbo_quads);  // 頂点バッファをバインド
	glBufferData(GL_ARRAY_BUFFER, sizeof(quadverts_t) * quad_buffer_len, quad_buffer, GL_DYNAMIC_DRAW);
	
	// インデックスバッファをバインド
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo_indices);
	
	// 描画実行（三角形として描画、インデックスを使用）
	glDrawElements(GL_TRIANGLES, quad_buffer_len * 6, GL_UNSIGNED_SHORT, 0);
	
	// バッファをクリア（次のフレームの準備）
	quad_buffer_len = 0;
}

// ブレンドモード（透明度の計算方法）の設定
// -----------------------------------------------------------------------------
// 【初心者向け解説】
// ブレンドモードは、新しく描画するピクセルと既に描画されている背景のピクセルを
// どのように合成するかを決める設定です。
//
// - 通常ブレンド（RENDER_BLEND_NORMAL）: 一般的な透明度効果
// - 加算ブレンド（RENDER_BLEND_LIGHTER）: 光や炎などの効果に使用、色が重なると明るくなる
void render_set_blend_mode(render_blend_mode_t new_mode) {
	// 同じモードなら何もしない
	if (new_mode == blend_mode) {
		return;
	}
	
	// ブレンドモードを変更する前に保留中の描画を実行
	render_flush();

	// 新しいブレンドモードを設定
	blend_mode = new_mode;
	
	// OpenGLのブレンド関数を設定
	if (blend_mode == RENDER_BLEND_NORMAL) {
		// 通常ブレンド: アルファ値に基づいて背景と混ぜる
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}
	else if (blend_mode == RENDER_BLEND_LIGHTER) {
		// 加算ブレンド: 色を加算する（明るい効果）
		glBlendFunc(GL_SRC_ALPHA, GL_ONE);
	}
}

// 四角形の描画（レンダラーバックエンドの内部関数）
// -----------------------------------------------------------------------------
// 【初心者向け解説】
// この関数は、render.cのrender_draw関数から呼び出され、四角形のデータを
// 描画バッファに追加します。実際の描画はrender_flush関数で行われます。
//
// テクスチャの座標をテクスチャアトラス内の位置に変換する処理が主な役割です。
void render_draw_quad(quadverts_t *quad, texture_t texture_handle) {
	// テクスチャハンドルの有効性をチェック
	error_if(texture_handle.index >= textures_len, "Invalid texture %d", texture_handle.index);
	
	// テクスチャのアトラス内位置情報を取得
	atlas_pos_t *t = &textures[texture_handle.index];

	// バッファが一杯なら先に描画を実行
	if (quad_buffer_len >= RENDER_BUFFER_CAPACITY) {
		render_flush();
	}

	// 四角形データをバッファにコピー
	quad_buffer[quad_buffer_len] = *quad;
	
	// テクスチャ座標を変換（0～1の範囲からアトラス内の実際の位置に）
	for (uint32_t i = 0; i < 4; i++) {
		// X座標の変換
		quad_buffer[quad_buffer_len].vertices[i].uv.x = 
			(quad_buffer[quad_buffer_len].vertices[i].uv.x + t->offset.x) * (1.0 / RENDER_ATLAS_SIZE_PX);
		// Y座標の変換
		quad_buffer[quad_buffer_len].vertices[i].uv.y = 
			(quad_buffer[quad_buffer_len].vertices[i].uv.y + t->offset.y) * (1.0 / RENDER_ATLAS_SIZE_PX);
	}
	
	// バッファ内の四角形数を増やす
	quad_buffer_len++;
}



// テクスチャ管理機能
// =============================================================================
// 【初心者向け解説】
// このセクションでは、テクスチャの作成、管理、更新に関する機能を提供します。
// テクスチャはすべて1つの大きなテクスチャアトラスに格納され、効率よく描画されます。

// 現在のテクスチャ状態をマークする
// -----------------------------------------------------------------------------
// 【初心者向け解説】
// 現在のテクスチャリストの状態を覚えておき、後で textures_reset で
// その状態に戻すことができます。一時的なテクスチャを使う場合に便利です。
texture_mark_t textures_mark(void) {
	return (texture_mark_t){.index = textures_len};  // 現在のテクスチャ数を記録
}

// テクスチャをマークした時点の状態にリセットする
// -----------------------------------------------------------------------------
// 【初心者向け解説】
// 以前にテクスチャをマークした状態に戻します。
// マーク以降に作成されたすべてのテクスチャが削除されます。
// 例えば、ゲームのレベルを切り替えるときに、前のレベルで使っていた
// テクスチャを解放するのに使用できます。
void textures_reset(texture_mark_t mark) {
	// マークが有効かチェック
	error_if(mark.index > textures_len, "Invalid texture reset mark %d >= %d", mark.index, textures_len);
	
	// 既に指定された状態にある場合は何もしない
	if (mark.index == textures_len) {
		return;
	}
	
	// 保留中の描画を実行
	render_flush();

	// テクスチャ数をマークした時点まで戻す
	textures_len = mark.index;
	
	// アトラスマップをクリア（再構築用）
	clear(atlas_map);

	// すべてのテクスチャをクリアする場合
	if (textures_len == 0) {
		// 基本テクスチャ（白）を再作成
		rgba_t white_pixels[4] = {rgba_white(), rgba_white(), rgba_white(), rgba_white()};
		RENDER_NO_TEXTURE = texture_create(vec2i(2, 2), white_pixels);
		return;
	}

	// テクスチャグリッドマップを再構築（残ったテクスチャを基に）
	for (int i = 0; i < textures_len; i++) {
		// テクスチャのグリッド位置とサイズを計算
		uint32_t grid_x = (textures[i].offset.x - RENDER_ATLAS_BORDER) / RENDER_ATLAS_GRID;
		uint32_t grid_y = (textures[i].offset.y - RENDER_ATLAS_BORDER) / RENDER_ATLAS_GRID;
		uint32_t grid_width = (textures[i].size.x + RENDER_ATLAS_BORDER * 2 + RENDER_ATLAS_GRID - 1) / RENDER_ATLAS_GRID;
		uint32_t grid_height = (textures[i].size.y + RENDER_ATLAS_BORDER * 2 + RENDER_ATLAS_GRID - 1) / RENDER_ATLAS_GRID;
		
		// アトラスマップを更新（使用済み領域をマーク）
		for (uint32_t cx = grid_x; cx < grid_x + grid_width; cx++) {
			atlas_map[cx] = grid_y + grid_height;
		}
	}
}

// 新しいテクスチャを作成する
// -----------------------------------------------------------------------------
// 【初心者向け解説】
// ピクセルデータから新しいテクスチャを作成し、テクスチャアトラスに追加します。
// この関数は画像ファイルをロードした後、その画像データをテクスチャとして
// GPUに転送するために使用されます。
texture_t texture_create(vec2i_t size, rgba_t *pixels) {
	// テクスチャ最大数のチェック
	error_if(textures_len >= RENDER_TEXTURES_MAX, "RENDER_TEXTURES_MAX reached");

	// 境界を含めたサイズを計算
	uint32_t bw = size.x + RENDER_ATLAS_BORDER * 2;  // 幅 + 境界
	uint32_t bh = size.y + RENDER_ATLAS_BORDER * 2;  // 高さ + 境界

	// テクスチャのグリッドサイズを計算（グリッド単位）
	uint32_t grid_width = (bw + RENDER_ATLAS_GRID - 1) / RENDER_ATLAS_GRID;   // 幅（切り上げ）
	uint32_t grid_height = (bh + RENDER_ATLAS_GRID - 1) / RENDER_ATLAS_GRID;  // 高さ（切り上げ）
	uint32_t grid_x = 0;  // 配置位置X（これから探す）
	uint32_t grid_y = RENDER_ATLAS_SIZE - grid_height + 1;  // 配置位置Y（初期値は最大値）

	// テクスチャがアトラスに収まるかチェック
	error_if(grid_width > RENDER_ATLAS_SIZE || grid_height > RENDER_ATLAS_SIZE, 
		"Texture of size %dx%d doesn't fit in atlas", size.x, size.y);

	// アトラス内の空き位置を探す
	for (uint32_t cx = 0; cx < RENDER_ATLAS_SIZE - grid_width; cx++) {
		// 現在位置に入らない場合はスキップ
		if (atlas_map[cx] >= grid_y) {
			continue;
		}

		// 現在列の最も高い位置
		uint32_t cy = atlas_map[cx];
		bool is_best = true;

		// この幅に収まるか確認
		for (uint32_t bx = cx; bx < cx + grid_width; bx++) {
			// 既に使用されている位置がある場合
			if (atlas_map[bx] >= grid_y) {
				is_best = false;
				cx = bx;  // 次の確認位置を更新
				break;
			}
			// より高い位置がある場合（最も高い位置を使用）
			if (atlas_map[bx] > cy) {
				cy = atlas_map[bx];
			}
		}
		
		// 最適な位置が見つかった場合
		if (is_best) {
			grid_y = cy;  // Y位置を更新
			grid_x = cx;  // X位置を更新
		}
	}

	// アトラスの空き容量を超えていないかチェック
	error_if(grid_y + grid_height > RENDER_ATLAS_SIZE, 
		"Render atlas ran out of space for %dx%d texture", size.x, size.y);

	// 使用領域をマーク
	for (uint32_t cx = grid_x; cx < grid_x + grid_width; cx++) {
		atlas_map[cx] = grid_y + grid_height;
	}

	// ピクセル座標に変換
	uint32_t x = grid_x * RENDER_ATLAS_GRID;
	uint32_t y = grid_y * RENDER_ATLAS_GRID;
	
	// テクスチャアトラスをバインド
	glBindTexture(GL_TEXTURE_2D, atlas_texture);

	// テクスチャに境界ピクセルを追加（補間時の問題を防ぐため）
	#if RENDER_ATLAS_BORDER > 0
		// 境界を含めたピクセルデータを確保
		rgba_t *pb = temp_alloc(sizeof(rgba_t) * bw * bh);

		if (size.x && size.y) {
			// 上部境界（最上段のピクセルを複製）
			for (int32_t y = 0; y < RENDER_ATLAS_BORDER; y++) {
				memcpy(pb + bw * y + RENDER_ATLAS_BORDER, pixels, size.x * sizeof(rgba_t));
			}

			// 下部境界（最下段のピクセルを複製）
			for (int32_t y = 0; y < RENDER_ATLAS_BORDER; y++) {
				memcpy(pb + bw * (bh - RENDER_ATLAS_BORDER + y) + RENDER_ATLAS_BORDER, 
					   pixels + size.x * (size.y-1), size.x * sizeof(rgba_t));
			}
			
			// 左境界（左端のピクセルを複製）
			for (int32_t y = 0; y < bh; y++) {
				for (int32_t x = 0; x < RENDER_ATLAS_BORDER; x++) {
					pb[y * bw + x] = pixels[clamp(y-RENDER_ATLAS_BORDER, 0, size.y-1) * size.x];
				}
			}

			// 右境界（右端のピクセルを複製）
			for (int32_t y = 0; y < bh; y++) {
				for (int32_t x = 0; x < RENDER_ATLAS_BORDER; x++) {
					pb[y * bw + x + bw - RENDER_ATLAS_BORDER] = 
						pixels[size.x - 1 + clamp(y-RENDER_ATLAS_BORDER, 0, size.y-1) * size.x];
				}
			}

			// 本体テクスチャ（中央部分）
			for (int32_t y = 0; y < size.y; y++) {
				memcpy(pb + bw * (y + RENDER_ATLAS_BORDER) + RENDER_ATLAS_BORDER, 
					   pixels + size.x * y, size.x * sizeof(rgba_t));
			}
		}

		// 境界を含めたテクスチャデータをGPUに転送
		glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, bw, bh, GL_RGBA, GL_UNSIGNED_BYTE, pb);
		temp_free(pb);  // 一時メモリを解放
	#else
		// 境界なしの場合は直接テクスチャデータを転送
		glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, bw, bh, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
	#endif

	// ミップマップ更新フラグを設定（有効な場合）
	mipmap_is_dirty = RENDER_USE_MIPMAPS;
	
	// テクスチャハンドルの作成と登録
	texture_t texture_handle = {.index = textures_len};
	textures_len++;  // テクスチャ数を増やす
	
	// テクスチャ情報を保存
	textures[texture_handle.index] = (atlas_pos_t){
		.offset = {x + RENDER_ATLAS_BORDER, y + RENDER_ATLAS_BORDER},  // アトラス内位置（境界除く）
		.size = size  // オリジナルサイズ
	};
	
	return texture_handle;  // テクスチャハンドルを返す
}

// 既存テクスチャのピクセルデータを更新する
// -----------------------------------------------------------------------------
// 【初心者向け解説】
// 既に作成されたテクスチャの内容を新しいピクセルデータで置き換えます。
// アニメーションや動的なテクスチャを実現するために使用できます。
void texture_replace_pixels(texture_t texture_handle, vec2i_t size, rgba_t *pixels) {
	// テクスチャハンドルの有効性をチェック
	error_if(texture_handle.index >= textures_len, "Invalid texture %d", texture_handle.index);

	// テクスチャ情報を取得
	atlas_pos_t *t = &textures[texture_handle.index];
	
	// 置き換えサイズが元のテクスチャより大きくないことを確認
	error_if(t->size.x < size.x || t->size.y < size.y, 
		"Cannot replace %dx%d pixels of %dx%d texture", size.x, size.y, t->size.x, t->size.y);

	// テクスチャアトラスをバインド
	glBindTexture(GL_TEXTURE_2D, atlas_texture);
	
	// 新しいピクセルデータをGPUに転送
	glTexSubImage2D(GL_TEXTURE_2D, 0, t->offset.x, t->offset.y, size.x, size.y, 
					GL_RGBA, GL_UNSIGNED_BYTE, pixels);
}

// アトラス内容をデバッグ用に画像ファイルに保存（コメントアウト中）
// -----------------------------------------------------------------------------
// void textures_dump(const char *path) {
// 	int width = RENDER_ATLAS_SIZE * RENDER_ATLAS_GRID;
// 	int height = RENDER_ATLAS_SIZE * RENDER_ATLAS_GRID;
// 	rgba_t *pixels = malloc(sizeof(rgba_t) * width * height);
// 	glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
// 	stbi_write_png(path, width, height, 4, pixels, 0);
// 	free(pixels);
// }
