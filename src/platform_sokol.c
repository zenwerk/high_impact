// Sokol（簡易ゲームライブラリ）を使用したプラットフォーム実装
// -----------------------------------------------------------------------------
// 【初心者向け解説】
// Sokolは、「sokol_app」「sokol_audio」「sokol_time」などの小さなライブラリ群で
// 構成される軽量なゲーム開発ツールキットです。
//
// Sokolの主な特徴：
// - 非常にシンプルで小さいコードベース
// - WebGL/WebAssemblyのサポートが優れている（ブラウザで動作可能）
// - 学習曲線が緩やか
// - 複数のプラットフォームをサポート（Windows、macOS、Linux、Web）
// - ヘッダーファイルのみで実装されている（使いやすい）
//
// SDLとの主な違い：
// - Sokolはより軽量で、特にWebブラウザでの動作に優れている
// - SDLの方が多機能で、より多くのプラットフォームやハードウェアをサポート
// - このエンジンでは両方をサポートし、用途に応じて選択できる
//
// このファイルはhigh_impactエンジンのプラットフォーム抽象化レイヤーを
// Sokolを使って実装しています。
#include "platform.h"
#include "sound.h"
#include "input.h"
#include "engine.h"
#include "utils.h"
#include "alloc.h"

// レンダラーの選択
// -----------------------------------------------------------------------------
// 【初心者向け解説】
// コンパイル時に選択されたレンダラーに基づいて、適切なSokolの
// グラフィックスバックエンドを定義します。
//
// - RENDER_GL: OpenGLを使用（クロスプラットフォーム3Dグラフィックス）
//   - __EMSCRIPTEN__: ブラウザ向けの場合はOpenGL ES 3を使用
//   - それ以外: デスクトップ向けのOpenGL Coreプロファイルを使用
// - RENDER_METAL: Apple製デバイス向けのMetalを使用
#if defined(RENDER_GL)
	#ifdef __EMSCRIPTEN__
		#define SOKOL_GLES3  // ブラウザ向けのOpenGL ES 3
	#else
		#define SOKOL_GLCORE // デスクトップ向けのOpenGL
	#endif
#elif defined(RENDER_METAL)
	#define SOKOL_METAL  // Apple製デバイス向けのMetal
#else
	#error "Unsupported renderer for platform SOKOL"
#endif

// Sokolライブラリの実装を含める
// -----------------------------------------------------------------------------
// 【初心者向け解説】
// Sokolはヘッダーオンリーのライブラリです。SOKOL_IMPLを定義することで、
// ヘッダーファイルに実装コードも含めます。これはC言語の一般的なテクニックで、
// 「ヘッダーオンリーライブラリ」と呼ばれます。
#define SOKOL_IMPL
#include "../libs/sokol_audio.h"  // オーディオ処理
#include "../libs/sokol_time.h"   // 時間計測
#include "../libs/sokol_app.h"    // ウィンドウ作成と入力処理
#include "input.h"

// QOPアセットパッケージ形式の実装を有効化
// -----------------------------------------------------------------------------
// 【初心者向け解説】
// QOPは実行ファイルに複数のアセットファイル（画像、音声など）を
// 埋め込むための簡易パッケージ形式です。実行ファイルと一緒に配布するだけで、
// 個別のファイルをインストールする必要がなくなります。
#define QOP_IMPLEMENTATION
#include "../libs/qop.h"

// グローバル変数
// -----------------------------------------------------------------------------
// 【初心者向け解説】
// これらはSokol実装で使用されるグローバル変数です。static修飾子を使うことで、
// このファイル内でのみアクセス可能な「プライベート」変数になります。
static char *path_assets;        // アセットファイルへのパス
static char *path_userdata;      // ユーザーデータファイルへのパス
static char *temp_path = NULL;   // 一時的なパス作成用バッファ
static qop_desc qop = {0};       // QOPアセットパッケージの記述子

// オーディオ出力サンプルレート（CD品質）
static uint32_t platform_output_samplerate = 44100;

// キーボードマッピングテーブル
// -----------------------------------------------------------------------------
// 【初心者向け解説】
// このマップは、SokolのキーコードをエンジンのINPUT_KEY_*定数に変換します。
// プラットフォーム抽象化の重要な部分で、Sokolの入力システムをエンジンの
// 入力システムに接続します。
//
// キーコードマッピングによって、エンジンは異なるプラットフォーム実装（SDL/Sokol）
// からの入力を一貫した方法で処理できます。異なるバックエンドを使用しても、
// ゲームコードは同じ入力定数を使用できます。
//
// C言語では配列の初期化で [インデックス] = 値 という構文を使うことで、
// 特定のインデックスに値を設定できます。
static const uint8_t keyboard_map[] = {
	// 標準キー
	[SAPP_KEYCODE_SPACE] = INPUT_KEY_SPACE,
	[SAPP_KEYCODE_APOSTROPHE] = INPUT_KEY_APOSTROPHE,
	[SAPP_KEYCODE_COMMA] = INPUT_KEY_COMMA,
	[SAPP_KEYCODE_MINUS] = INPUT_KEY_MINUS,
	[SAPP_KEYCODE_PERIOD] = INPUT_KEY_PERIOD,
	[SAPP_KEYCODE_SLASH] = INPUT_KEY_SLASH,
	
	// 数字キー
	[SAPP_KEYCODE_0] = INPUT_KEY_0,
	[SAPP_KEYCODE_1] = INPUT_KEY_1,
	[SAPP_KEYCODE_2] = INPUT_KEY_2,
	[SAPP_KEYCODE_3] = INPUT_KEY_3,
	[SAPP_KEYCODE_4] = INPUT_KEY_4,
	[SAPP_KEYCODE_5] = INPUT_KEY_5,
	[SAPP_KEYCODE_6] = INPUT_KEY_6,
	[SAPP_KEYCODE_7] = INPUT_KEY_7,
	[SAPP_KEYCODE_8] = INPUT_KEY_8,
	[SAPP_KEYCODE_9] = INPUT_KEY_9,
	
	// 記号キー
	[SAPP_KEYCODE_SEMICOLON] = INPUT_KEY_SEMICOLON,
	[SAPP_KEYCODE_EQUAL] = INPUT_KEY_EQUALS,
	
	// アルファベットキー
	[SAPP_KEYCODE_A] = INPUT_KEY_A,
	[SAPP_KEYCODE_B] = INPUT_KEY_B,
	[SAPP_KEYCODE_C] = INPUT_KEY_C,
	[SAPP_KEYCODE_D] = INPUT_KEY_D,
	[SAPP_KEYCODE_E] = INPUT_KEY_E,
	[SAPP_KEYCODE_F] = INPUT_KEY_F,
	[SAPP_KEYCODE_G] = INPUT_KEY_G,
	[SAPP_KEYCODE_H] = INPUT_KEY_H,
	[SAPP_KEYCODE_I] = INPUT_KEY_I,
	[SAPP_KEYCODE_J] = INPUT_KEY_J,
	[SAPP_KEYCODE_K] = INPUT_KEY_K,
	[SAPP_KEYCODE_L] = INPUT_KEY_L,
	[SAPP_KEYCODE_M] = INPUT_KEY_M,
	[SAPP_KEYCODE_N] = INPUT_KEY_N,
	[SAPP_KEYCODE_O] = INPUT_KEY_O,
	[SAPP_KEYCODE_P] = INPUT_KEY_P,
	[SAPP_KEYCODE_Q] = INPUT_KEY_Q,
	[SAPP_KEYCODE_R] = INPUT_KEY_R,
	[SAPP_KEYCODE_S] = INPUT_KEY_S,
	[SAPP_KEYCODE_T] = INPUT_KEY_T,
	[SAPP_KEYCODE_U] = INPUT_KEY_U,
	[SAPP_KEYCODE_V] = INPUT_KEY_V,
	[SAPP_KEYCODE_W] = INPUT_KEY_W,
	[SAPP_KEYCODE_X] = INPUT_KEY_X,
	[SAPP_KEYCODE_Y] = INPUT_KEY_Y,
	[SAPP_KEYCODE_Z] = INPUT_KEY_Z,
	
	// ブラケット/特殊キー
	[SAPP_KEYCODE_LEFT_BRACKET] = INPUT_KEY_LEFTBRACKET,
	[SAPP_KEYCODE_BACKSLASH] = INPUT_KEY_BACKSLASH,
	[SAPP_KEYCODE_RIGHT_BRACKET] = INPUT_KEY_RIGHTBRACKET,
	[SAPP_KEYCODE_GRAVE_ACCENT] = INPUT_KEY_TILDE,
	[SAPP_KEYCODE_WORLD_1] = INPUT_INVALID,				// 未実装
	[SAPP_KEYCODE_WORLD_2] = INPUT_INVALID,				// 未実装
	
	// 制御キー
	[SAPP_KEYCODE_ESCAPE] = INPUT_KEY_ESCAPE,
	[SAPP_KEYCODE_ENTER] = INPUT_KEY_RETURN,
	[SAPP_KEYCODE_TAB] = INPUT_KEY_TAB,
	[SAPP_KEYCODE_BACKSPACE] = INPUT_KEY_BACKSPACE,
	[SAPP_KEYCODE_INSERT] = INPUT_KEY_INSERT,
	[SAPP_KEYCODE_DELETE] = INPUT_KEY_DELETE,
	
	// 方向キー
	[SAPP_KEYCODE_RIGHT] = INPUT_KEY_RIGHT,
	[SAPP_KEYCODE_LEFT] = INPUT_KEY_LEFT,
	[SAPP_KEYCODE_DOWN] = INPUT_KEY_DOWN,
	[SAPP_KEYCODE_UP] = INPUT_KEY_UP,
	[SAPP_KEYCODE_PAGE_UP] = INPUT_KEY_PAGEUP,
	[SAPP_KEYCODE_PAGE_DOWN] = INPUT_KEY_PAGEDOWN,
	[SAPP_KEYCODE_HOME] = INPUT_KEY_HOME,
	[SAPP_KEYCODE_END] = INPUT_KEY_END,
	
	// ロックキー
	[SAPP_KEYCODE_CAPS_LOCK] = INPUT_KEY_CAPSLOCK,
	[SAPP_KEYCODE_SCROLL_LOCK] = INPUT_KEY_SCROLLLOCK,
	[SAPP_KEYCODE_NUM_LOCK] = INPUT_KEY_NUMLOCK,
	[SAPP_KEYCODE_PRINT_SCREEN] = INPUT_KEY_PRINTSCREEN,
	[SAPP_KEYCODE_PAUSE] = INPUT_KEY_PAUSE,
	
	// ファンクションキー
	[SAPP_KEYCODE_F1] = INPUT_KEY_F1,
	[SAPP_KEYCODE_F2] = INPUT_KEY_F2,
	[SAPP_KEYCODE_F3] = INPUT_KEY_F3,
	[SAPP_KEYCODE_F4] = INPUT_KEY_F4,
	[SAPP_KEYCODE_F5] = INPUT_KEY_F5,
	[SAPP_KEYCODE_F6] = INPUT_KEY_F6,
	[SAPP_KEYCODE_F7] = INPUT_KEY_F7,
	[SAPP_KEYCODE_F8] = INPUT_KEY_F8,
	[SAPP_KEYCODE_F9] = INPUT_KEY_F9,
	[SAPP_KEYCODE_F10] = INPUT_KEY_F10,
	[SAPP_KEYCODE_F11] = INPUT_KEY_F11,
	[SAPP_KEYCODE_F12] = INPUT_KEY_F12,
	[SAPP_KEYCODE_F13] = INPUT_INVALID, 				// 未実装
	[SAPP_KEYCODE_F14] = INPUT_INVALID, 				// 未実装
	[SAPP_KEYCODE_F15] = INPUT_INVALID, 				// 未実装
	[SAPP_KEYCODE_F16] = INPUT_INVALID, 				// 未実装
	[SAPP_KEYCODE_F17] = INPUT_INVALID, 				// 未実装
	[SAPP_KEYCODE_F18] = INPUT_INVALID, 				// 未実装
	[SAPP_KEYCODE_F19] = INPUT_INVALID, 				// 未実装
	[SAPP_KEYCODE_F20] = INPUT_INVALID, 				// 未実装
	[SAPP_KEYCODE_F21] = INPUT_INVALID, 				// 未実装
	[SAPP_KEYCODE_F22] = INPUT_INVALID, 				// 未実装
	[SAPP_KEYCODE_F23] = INPUT_INVALID, 				// 未実装
	[SAPP_KEYCODE_F24] = INPUT_INVALID, 				// 未実装
	[SAPP_KEYCODE_F25] = INPUT_INVALID, 				// 未実装
	
	// テンキー
	[SAPP_KEYCODE_KP_0] = INPUT_KEY_KP_0,
	[SAPP_KEYCODE_KP_1] = INPUT_KEY_KP_1,
	[SAPP_KEYCODE_KP_2] = INPUT_KEY_KP_2,
	[SAPP_KEYCODE_KP_3] = INPUT_KEY_KP_3,
	[SAPP_KEYCODE_KP_4] = INPUT_KEY_KP_4,
	[SAPP_KEYCODE_KP_5] = INPUT_KEY_KP_5,
	[SAPP_KEYCODE_KP_6] = INPUT_KEY_KP_6,
	[SAPP_KEYCODE_KP_7] = INPUT_KEY_KP_7,
	[SAPP_KEYCODE_KP_8] = INPUT_KEY_KP_8,
	[SAPP_KEYCODE_KP_9] = INPUT_KEY_KP_9,
	[SAPP_KEYCODE_KP_DECIMAL] = INPUT_KEY_KP_PERIOD,
	[SAPP_KEYCODE_KP_DIVIDE] = INPUT_KEY_KP_DIVIDE,
	[SAPP_KEYCODE_KP_MULTIPLY] = INPUT_KEY_KP_MULTIPLY,
	[SAPP_KEYCODE_KP_SUBTRACT] = INPUT_KEY_KP_MINUS,
	[SAPP_KEYCODE_KP_ADD] = INPUT_KEY_KP_PLUS,
	[SAPP_KEYCODE_KP_ENTER] = INPUT_KEY_KP_ENTER,
	[SAPP_KEYCODE_KP_EQUAL] = INPUT_INVALID, 			// 未実装
	
	// 修飾キー
	[SAPP_KEYCODE_LEFT_SHIFT] = INPUT_KEY_L_SHIFT,
	[SAPP_KEYCODE_LEFT_CONTROL] = INPUT_KEY_L_CTRL,
	[SAPP_KEYCODE_LEFT_ALT] = INPUT_KEY_L_ALT,
	[SAPP_KEYCODE_LEFT_SUPER] = INPUT_INVALID, 			// 未実装
	[SAPP_KEYCODE_RIGHT_SHIFT] = INPUT_KEY_R_SHIFT,
	[SAPP_KEYCODE_RIGHT_CONTROL] = INPUT_KEY_R_CTRL,
	[SAPP_KEYCODE_RIGHT_ALT] = INPUT_KEY_R_ALT,
	[SAPP_KEYCODE_RIGHT_SUPER] = INPUT_INVALID, 		// 未実装
	[SAPP_KEYCODE_MENU] = INPUT_INVALID, 				// 未実装
};


// オーディオミキシングコールバック関数ポインタ
// エンジンから設定されるオーディオ処理関数を格納する変数
static void (*audio_callback)(float *buffer, uint32_t len) = NULL;

// プログラムを終了する
// -----------------------------------------------------------------------------
// 【初心者向け解説】
// この関数は、アプリケーションが終了を要求したときに呼び出されます。
// Sokolアプリケーションを適切に終了させるためにsapp_quit()を呼び出します。
void platform_exit(void) {
	sapp_quit();
}

// 画面サイズの取得
// -----------------------------------------------------------------------------
// 【初心者向け解説】
// この関数は、現在のウィンドウの幅と高さをピクセル単位で返します。
// この情報は、レンダリングやレイアウトの計算に必要です。
vec2i_t platform_screen_size(void) {
	return vec2i(sapp_width(), sapp_height());
}

// 現在の時刻を秒単位で取得
// -----------------------------------------------------------------------------
// 【初心者向け解説】
// この関数は、Sokolの時間計測機能を使用して現在の時刻を秒単位で返します。
// stm_now()は現在の時刻「ティック」を取得し、stm_sec()はそれを秒に変換します。
// ゲーム内のアニメーションや物理計算に使用されます。
double platform_now(void) {
	return stm_sec(stm_now());
}

// フルスクリーンモードかどうかを確認
// -----------------------------------------------------------------------------
// 【初心者向け解説】
// この関数は、現在のウィンドウがフルスクリーンモードで
// 表示されているかどうかを確認します。
bool platform_get_fullscreen(void) {
	return sapp_is_fullscreen();
}

// フルスクリーンモードの切り替え
// -----------------------------------------------------------------------------
// 【初心者向け解説】
// この関数は、ウィンドウをフルスクリーンモードと通常モードの間で
// 切り替えます。また、フルスクリーン時にはマウスカーソルを非表示にします。
//
// 現在の状態が要求された状態と同じ場合は、何もしません。
void platform_set_fullscreen(bool fullscreen) {
	// すでに要求された状態なら何もしない
	if (fullscreen == sapp_is_fullscreen()) {
		return;
	}

	// フルスクリーンの切り替え
	sapp_toggle_fullscreen();
	// フルスクリーン時はマウスを非表示に、そうでない場合は表示する
	sapp_show_mouse(!fullscreen);
}

// Sokolイベントの処理
// -----------------------------------------------------------------------------
// 【初心者向け解説】
// この関数は、Sokolからのすべてのイベント（キーボード、マウス、ウィンドウなど）を
// 処理し、エンジンの入力システムに転送します。
//
// sokol_appは、アプリケーションがイベント順に処理するためにsapp_event構造体を
// この関数に渡します。SDLとは異なり、この関数はイベントキューを取得しません。
// 代わりに、Sokolがイベントを検出するたびにこの関数が呼び出されます。
void platform_handle_event(const sapp_event* ev) {
	// ALT+Enterキーでフルスクリーン切り替え
	// -------------------------------------
	if (
		ev->type == SAPP_EVENTTYPE_KEY_DOWN && 
		ev->key_code == SAPP_KEYCODE_ENTER &&
		(ev->modifiers & SAPP_MODIFIER_ALT)
	) {
		platform_set_fullscreen(!sapp_is_fullscreen());
	}

	// キーボード入力処理
	// -------------------------------------
	else if (ev->type == SAPP_EVENTTYPE_KEY_DOWN || ev->type == SAPP_EVENTTYPE_KEY_UP) {
		// キーが押された場合は1.0、離された場合は0.0
		float state = ev->type == SAPP_EVENTTYPE_KEY_DOWN ? 1.0 : 0.0;
		// キーコードが有効な範囲内にあるか確認
		if (ev->key_code > 0 && ev->key_code < sizeof(keyboard_map)) {
			// Sokolのキーコードをエンジン内部のコードに変換
			int code = keyboard_map[ev->key_code];
			// エンジンの入力システムにキーの状態を転送
			input_set_button_state(code, state);
		}
	}

	// テキスト入力処理
	// -------------------------------------
	else if (ev->type == SAPP_EVENTTYPE_CHAR) {
		// Unicode文字コードをエンジンに転送
		input_textinput(ev->char_code);
	}


	// ゲームパッド入力処理
	// -------------------------------------
	// TODO: Sokolライブラリ自体で未実装

	// マウスボタン入力処理
	// -------------------------------------
	else if (
		ev->type == SAPP_EVENTTYPE_MOUSE_DOWN ||
		ev->type == SAPP_EVENTTYPE_MOUSE_UP
	) {
		// Sokolのマウスボタンをエンジンのボタン定数に変換
		button_t button = INPUT_BUTTON_NONE;
		switch (ev->mouse_button) {
			case SAPP_MOUSEBUTTON_LEFT: button = INPUT_MOUSE_LEFT; break;
			case SAPP_MOUSEBUTTON_MIDDLE: button = INPUT_MOUSE_MIDDLE; break;
			case SAPP_MOUSEBUTTON_RIGHT: button = INPUT_MOUSE_RIGHT; break;
			default: break;
		}
		if (button != INPUT_BUTTON_NONE) {
			// ボタンが押された場合は1.0、離された場合は0.0
			float state = ev->type == SAPP_EVENTTYPE_MOUSE_DOWN ? 1.0 : 0.0;
			// エンジンの入力システムにボタンの状態を転送
			input_set_button_state(button, state);
		}
	}

	// マウスホイール処理
	// -------------------------------------
	else if (ev->type == SAPP_EVENTTYPE_MOUSE_SCROLL) {
		// ホイールの方向に応じてボタンを選択
		button_t button = ev->scroll_y > 0 
			? INPUT_MOUSE_WHEEL_UP    // 上方向
			: INPUT_MOUSE_WHEEL_DOWN; // 下方向
		// ホイールイベントは「クリック」扱いにするため、押して即離す
		input_set_button_state(button, 1.0);
		input_set_button_state(button, 0.0);
	}

	// マウス移動処理
	// -------------------------------------
	else if (ev->type == SAPP_EVENTTYPE_MOUSE_MOVE) {
		// エンジンの入力システムにマウス座標を転送
		input_set_mouse_pos(ev->mouse_x, ev->mouse_y);
	}

	// ウィンドウイベント処理
	// -------------------------------------
	if (ev->type == SAPP_EVENTTYPE_RESIZED) {
		// ウィンドウのサイズが変更された場合、エンジンに通知
		engine_resize(vec2i(ev->window_width, ev->window_height));
	}
}

// オーディオのサンプルレートを取得
// -----------------------------------------------------------------------------
// 【初心者向け解説】
// この関数は、現在のオーディオ出力のサンプルレート（1秒あたりのサンプル数）
// を返します。
//
// サンプルレートは音質に影響します。一般的な値は：
// - 44100 Hz（CD品質）
// - 48000 Hz（DVDやプロ用オーディオで一般的）
uint32_t platform_samplerate(void) {
	return platform_output_samplerate;
}

// Sokolオーディオコールバック
// -----------------------------------------------------------------------------
// 【初心者向け解説】
// この関数は、Sokolのオーディオシステムから定期的に呼び出されます。
// オーディオデバイスが新しいオーディオデータを要求するたびに呼び出され、
// エンジンのサウンドシステムからデータを生成します。
//
// Sokolのオーディオシステムは、バッファのフレーム数とチャンネル数を別々に指定します。
void platform_audio_callback(float* buffer, int num_frames, int num_channels) {
	if (audio_callback) {
		// エンジンのオーディオコールバックが設定されている場合はそれを使う
		// フレーム数 × チャンネル数 = 全サンプル数
		audio_callback(buffer, num_frames * num_channels);
	}
	else {
		// コールバックが設定されていない場合は無音を出力
		memset(buffer, 0, num_frames * sizeof(float));
	}
}

// オーディオミキシングコールバックを設定
// -----------------------------------------------------------------------------
// 【初心者向け解説】
// この関数は、エンジンのサウンドシステムからオーディオデータを取得するための
// コールバック関数を設定します。
//
// コールバックパターンとは、「後で呼び出してほしい関数」を登録しておく
// プログラミング手法です。オーディオプログラミングでは非常に一般的で、
// データが必要になったタイミングで関数が呼び出されます。
void platform_set_audio_mix_cb(void (*cb)(float *buffer, uint32_t len)) {
	// オーディオコールバック関数を保存
	audio_callback = cb;
}

// アセットファイルの読み込み
// -----------------------------------------------------------------------------
// 【初心者向け解説】
// この関数は、ゲームに必要なリソース（画像、音声、マップデータなど）を
// 読み込みます。読み込みは次の優先順で行われます：
//
// 1. QOPアーカイブ（実行ファイルに埋め込まれたアセット）
// 2. ファイルシステム上のアセットディレクトリ
//
// QOPパッケージはゲーム配布を簡単にするための便利な方法です。
uint8_t *platform_load_asset(const char *name, uint32_t *bytes_read) {
	// まずQOPアーカイブからの読み込みを試みる
	if (qop.index_len) {
		// アーカイブ内でファイルを検索
		qop_file *f = qop_find(&qop, name);
		if (f) {
			// 見つかった場合、一時メモリに読み込む
			uint8_t *data = temp_alloc(f->size);
			*bytes_read = qop_read(&qop, f, data);
			return data;
		}
	}

	// QOPアーカイブにない場合はファイルシステムから読み込む
	// アセットパスとファイル名を結合
	char *path = strcat(strcpy(temp_path, path_assets), name);
	return file_load(path, bytes_read);
}

// ユーザーデータファイルの読み込み
// -----------------------------------------------------------------------------
// 【初心者向け解説】
// この関数は、ユーザー固有のデータ（セーブデータ、設定など）を
// 読み込みます。ユーザーデータは常にファイルシステムから読み込まれ、
// QOPアーカイブには含まれません。
//
// ファイルが存在しない場合はエラーにはせず、NULLを返します。
uint8_t *platform_load_userdata(const char *name, uint32_t *bytes_read) {
	// ユーザーデータパスとファイル名を結合
	char *path = strcat(strcpy(temp_path, path_userdata), name);
	// ファイルが存在しない場合はNULLを返す
	if (!file_exists(path)) {
		*bytes_read = 0;
		return NULL;
	}
	// ファイルを読み込む
	return file_load(path, bytes_read);
}

// ユーザーデータファイルの保存
// -----------------------------------------------------------------------------
// 【初心者向け解説】
// この関数は、ユーザー固有のデータ（セーブデータ、設定など）を
// ファイルに保存します。保存先は、platform_load_userdata関数と同じ
// パスが使用されます。
//
// 返値は、実際に書き込まれたバイト数です。
uint32_t platform_store_userdata(const char *name, void *bytes, int32_t len) {
	// ユーザーデータパスとファイル名を結合
	char *path = strcat(strcpy(temp_path, path_userdata), name);
	// ファイルにデータを書き込む
	return file_store(path, bytes, len);
}

// プラットフォームリソースのクリーンアップ
// -----------------------------------------------------------------------------
// 【初心者向け解説】
// この関数は、プログラム終了時に呼び出され、確保したすべてのリソースを
// 解放します。メモリリーク（メモリ未解放）を防ぐために重要です。
//
// 解放の順序が重要な場合があります。例えば、サウンドシステムは
// エンジンの他の部分を使用している可能性があるため、エンジンのクリーンアップ後に
// 終了する必要があります。
void platform_cleanup(void) {
	// エンジンのクリーンアップ
	engine_cleanup();
	// サウンドシステムのクリーンアップ
	sound_cleanup();
	// Sokolオーディオシステムの㊷ャットダウン
	saudio_shutdown();

	// QOPアーカイブが開かれていれば閉じる
	if (qop.index_len) {
		qop_close(&qop);
	}
}

#if defined(RENDER_METAL)
// Metalレイヤーへのアクセスを提供
// -----------------------------------------------------------------------------
// 【初心者向け解説】
// この関数は、Metalレンダラーに必要なCAMetalLayerのポインタを返します。
// これはmacOSやiOSでMetalグラフィックスAPIを使用する場合に必要です。
//
// __bridgeというキーワードは、Objective-CのオブジェクトをCのポインタに
// 変換するための特別な構文です。
void *platform_get_metal_layer(void) {
	return (__bridge void *)_sapp.macos.view.layer;
}
#endif

// Sokolアプリケーションのメインエントリーポイント
// =============================================================================
// 【初心者向け解説】
// この関数は、Sokolアプリケーションのメインエントリーポイントで、
// SDL実装のmain()関数に相当します。ここでは次のことを行います：
//
// 1. パスの解決と設定
// 2. QOPアーカイブの読み込み
// 3. オーディオの初期化
// 4. Sokolアプリケーション設定の初期化と返却
//
// sokol_main関数は、sapp_desc構造体を返します。この構造体は、
// ウィンドウのタイトルやサイズ、各種コールバック関数など、
// アプリケーションの動作を制御する設定を含んでいます。
sapp_desc sokol_main(int argc, char* argv[]) {
	// 実行可能ファイルのパスを解決
	// これは、コンパイル時にPATH_ASSETSまたはPATH_USERDATAが
	// 設定されていない場合のベースディレクトリとして使用されます
	// FIXME: path_userdataは、SDLのSDL_GetPrefPath()と同様に、
	// ホームディレクトリ内の場所を指すべきかもしれません
	char *exe_path = platform_executable_path();
	char *base_path = "";
	if (exe_path) {
		base_path = platform_dirname(exe_path);
	}

	// アセットパスの設定
	#ifdef PATH_ASSETS
		// コンパイル時に定義されていればそれを使用
		path_assets = TOSTRING(PATH_ASSETS);
	#else
		// 定義されていなければ実行ファイルのディレクトリを使用
		path_assets = base_path;
	#endif

	// ユーザーデータパスの設定
	#ifdef PATH_USERDATA
		// コンパイル時に定義されていればそれを使用
		path_userdata = TOSTRING(PATH_USERDATA);
	#else
		// 定義されていなければ実行ファイルのディレクトリを使用
		path_userdata = base_path;
	#endif

	// QOPアーカイブの読み込みを試みる
	// リリースビルドでは、実行可能ファイルにアセットが埋め込まれている可能性があります
	if (exe_path && qop_open(exe_path, &qop)) {
		printf("Opened QOP archive from %s; %d bytes, %d files\n", exe_path, qop.files_offset, qop.index_len);
		// QOPアーカイブのインデックスをメモリに読み込む
		qop_read_index(&qop, bump_alloc(qop.hashmap_size));
	}

	// パス結合用の一時バッファを確保
	temp_path = bump_alloc(max(strlen(path_assets), strlen(path_userdata)) + PLATFORM_MAX_PATH);

	// Sokolの時間測定機能を初期化
	stm_setup();
	
	// Sokolオーディオを初期化
	saudio_setup(&(saudio_desc){
		.sample_rate = platform_output_samplerate,  // サンプルレート（44.1kHz）
		.buffer_frames = 1024,                     // バッファサイズ（レイテンシと処理負荷のバランス）
		.num_channels = 2,                        // ステレオ出力（2チャンネル）
		.stream_cb = platform_audio_callback,      // オーディオ出力生成用コールバック
	});

	// 実際に使用されるサンプルレートを取得（要求と異なる場合がある）
	platform_output_samplerate = saudio_sample_rate();

	// Sokolアプリケーションの設定を返す
	return (sapp_desc) {
		.width = WINDOW_WIDTH,                 // 初期ウィンドウ幅
		.height = WINDOW_HEIGHT,                // 初期ウィンドウ高さ
		.init_cb = engine_init,                 // 初期化時に呼び出す関数
		.frame_cb = engine_update,              // 毎フレームで呼び出す関数
		.window_title = WINDOW_TITLE,           // ウィンドウタイトル
		.cleanup_cb = platform_cleanup,         // 終了時に呼び出す関数
		.event_cb = platform_handle_event,      // イベント処理関数
		.win32_console_attach = true            // Windowsでコンソールを表示
	};
}
