// SDL（Simple DirectMedia Layer）を使用したプラットフォーム実装
// -----------------------------------------------------------------------------
// 【初心者向け解説】
// SDL（Simple DirectMedia Layer）は、ウィンドウ作成、グラフィックス、入力、
// オーディオなどの低レベル機能を提供するクロスプラットフォームライブラリです。
// このファイルは、high_impactエンジンのプラットフォーム抽象化レイヤーをSDLを
// 使って実装しています。
//
// SDLの主な特徴：
// - 多くのプラットフォーム（Windows、macOS、Linux、iOS、Androidなど）をサポート
// - 成熟した多機能なマルチメディアライブラリ
// - 幅広いハードウェアへのアクセス（ゲームパッド、タッチスクリーンなど）
// - 長い歴史と大きなコミュニティからのサポート
//
// Sokolとの違い：
// - SDLはより機能が豊富だが、コードサイズが大きくなる
// - SDL単体ではWebブラウザ向けの実装が複雑
// - より多くのプラットフォームに対応している
//
// sdl-config in the makefile should put SDL.h into the header search path
// see: https://nullprogram.com/blog/2023/01/08/
#include "SDL.h" 

#include "platform.h"
#include "input.h"
#include "sound.h"
#include "engine.h"
#include "utils.h"
#include "alloc.h"

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
// これらはSDL実装で使用されるグローバル変数です。static修飾子を使うことで、
// このファイル内でのみアクセス可能な「プライベート」変数になります。
//
// これらの変数は、ウィンドウ、オーディオ、入力デバイス、ファイルパスなど、
// プログラム全体で必要となる重要な状態を保持します。
static uint64_t perf_freq = 0;                                       // パフォーマンスカウンターの周波数（時間計測用）
static bool wants_to_exit = false;                                   // プログラム終了フラグ
static SDL_Window *window;                                          // SDLウィンドウハンドル
static SDL_AudioDeviceID audio_device;                              // オーディオデバイスID
static SDL_GameController *gamepad;                                 // 接続されたゲームパッドへの参照
static void (*audio_callback)(float *buffer, uint32_t len) = NULL;  // オーディオミキシング用コールバック関数ポインタ
static char *path_assets = "";                                      // アセットファイルへのパス
static char *path_userdata = "";                                    // ユーザーデータファイルへのパス
static char *temp_path = NULL;                                      // 一時的なパス作成用バッファ
static uint32_t platform_output_samplerate = 44100;                 // オーディオ出力サンプルレート
static qop_desc qop = {0};                                          // QOPアセットパッケージの記述子


// ゲームパッドボタンのマッピング
// -----------------------------------------------------------------------------
// 【初心者向け解説】
// このマップは、SDLのゲームパッドボタン定数（SDL_CONTROLLER_BUTTON_*）を
// エンジン内部の入力定数（INPUT_GAMEPAD_*）に変換します。
//
// これにより、エンジンコードはSDL固有の定数に依存せず、抽象化された
// 入力システムを使用できます。異なるプラットフォーム（SDLやSokol）でも
// 同じ入力コードが動作します。
//
// C言語では配列の初期化で [インデックス] = 値 という構文を使うことで、
// 特定のインデックスに値を設定できます。
static const uint8_t platform_sdl_gamepad_map[] = {
	[SDL_CONTROLLER_BUTTON_A] = INPUT_GAMEPAD_A,               // Aボタン
	[SDL_CONTROLLER_BUTTON_B] = INPUT_GAMEPAD_B,               // Bボタン
	[SDL_CONTROLLER_BUTTON_X] = INPUT_GAMEPAD_X,               // Xボタン
	[SDL_CONTROLLER_BUTTON_Y] = INPUT_GAMEPAD_Y,               // Yボタン
	[SDL_CONTROLLER_BUTTON_BACK] = INPUT_GAMEPAD_SELECT,       // セレクト/バックボタン
	[SDL_CONTROLLER_BUTTON_GUIDE] = INPUT_GAMEPAD_HOME,        // ガイド/ホームボタン
	[SDL_CONTROLLER_BUTTON_START] = INPUT_GAMEPAD_START,       // スタートボタン
	[SDL_CONTROLLER_BUTTON_LEFTSTICK] = INPUT_GAMEPAD_L_STICK_PRESS,    // 左スティック押し込み
	[SDL_CONTROLLER_BUTTON_RIGHTSTICK] = INPUT_GAMEPAD_R_STICK_PRESS,   // 右スティック押し込み
	[SDL_CONTROLLER_BUTTON_LEFTSHOULDER] = INPUT_GAMEPAD_L_SHOULDER,    // 左ショルダーボタン
	[SDL_CONTROLLER_BUTTON_RIGHTSHOULDER] = INPUT_GAMEPAD_R_SHOULDER,   // 右ショルダーボタン
	[SDL_CONTROLLER_BUTTON_DPAD_UP] = INPUT_GAMEPAD_DPAD_UP,            // 十字キー上
	[SDL_CONTROLLER_BUTTON_DPAD_DOWN] = INPUT_GAMEPAD_DPAD_DOWN,        // 十字キー下
	[SDL_CONTROLLER_BUTTON_DPAD_LEFT] = INPUT_GAMEPAD_DPAD_LEFT,        // 十字キー左
	[SDL_CONTROLLER_BUTTON_DPAD_RIGHT] = INPUT_GAMEPAD_DPAD_RIGHT,      // 十字キー右
	[SDL_CONTROLLER_BUTTON_MAX] = INPUT_INVALID                         // 無効値（範囲外チェック用）
};


// ゲームパッドアナログスティック/トリガーのマッピング
// -----------------------------------------------------------------------------
// 【初心者向け解説】
// このマップは、SDLのゲームパッドアナログ軸定数（SDL_CONTROLLER_AXIS_*）を
// エンジン内部の入力定数に変換します。
//
// アナログスティックは2つの軸（X軸とY軸）があり、それぞれ-1.0から1.0の範囲の
// 値を持ちます。例えば、スティックを右に倒すとX軸は正の値になり、
// 左に倒すと負の値になります。
static const uint8_t platform_sdl_axis_map[] = {
	[SDL_CONTROLLER_AXIS_LEFTX] = INPUT_GAMEPAD_L_STICK_LEFT,       // 左スティックX軸
	[SDL_CONTROLLER_AXIS_LEFTY] = INPUT_GAMEPAD_L_STICK_UP,         // 左スティックY軸
	[SDL_CONTROLLER_AXIS_RIGHTX] = INPUT_GAMEPAD_R_STICK_LEFT,      // 右スティックX軸
	[SDL_CONTROLLER_AXIS_RIGHTY] = INPUT_GAMEPAD_R_STICK_UP,        // 右スティックY軸
	[SDL_CONTROLLER_AXIS_TRIGGERLEFT] = INPUT_GAMEPAD_L_TRIGGER,    // 左トリガー
	[SDL_CONTROLLER_AXIS_TRIGGERRIGHT] = INPUT_GAMEPAD_R_TRIGGER,   // 右トリガー
	[SDL_CONTROLLER_AXIS_MAX] = INPUT_INVALID                       // 無効値（範囲外チェック用）
};


// プログラムを終了する
// -----------------------------------------------------------------------------
// 【初心者向け解説】
// プログラムがユーザーの操作（ウィンドウを閉じるなど）または内部コード
// （エラー、ゲーム終了など）によって終了する必要がある場合に呼び出されます。
// この関数は終了フラグを設定するだけで、即座に終了するわけではありません。
// メインループが次のイテレーションでこのフラグをチェックして、クリーンアップ
// 処理を行ってから終了します。
void platform_exit(void) {
	wants_to_exit = true;
}

// 接続されたゲームパッドを検索
// -----------------------------------------------------------------------------
// 【初心者向け解説】
// この関数は、接続されているすべてのジョイスティックデバイスを調べ、
// ゲームコントローラーとして認識できるものを探します。
//
// ゲームパッドはさまざまな種類があり、ボタン配置も異なりますが、
// SDLのゲームコントローラーAPIは「標準的な」マッピングを提供します。
// これにより、どのメーカーのゲームパッドでも同じコードで操作できます。
SDL_GameController *platform_find_gamepad(void) {
	// 接続されているすべてのジョイスティックをチェック
	for (int i = 0; i < SDL_NumJoysticks(); i++) {
		// ジョイスティックがゲームコントローラーとして認識できるか確認
		if (SDL_IsGameController(i)) {
			// 見つかったら開いて返す
			return SDL_GameControllerOpen(i);
		}
	}

	// 見つからなければNULLを返す
	return NULL;
}


// イベント処理
// -----------------------------------------------------------------------------
// 【初心者向け解説】
// この関数は「イベントループ」の中心部分で、ユーザー入力（キーボード、マウス、
// ゲームパッド）やウィンドウイベント（リサイズ、閉じるなど）を処理します。
//
// ゲームやグラフィカルアプリケーションでは、ユーザーが何かアクションを起こすと
// 「イベント」が発生します。例えば、キーを押す、マウスを動かす、ウィンドウを
// リサイズするなどです。SDLはこれらのイベントをキューに入れ、このコードが
// それらを処理します。
//
// この関数は毎フレーム呼び出され、キューにあるすべてのイベントを処理します。
void platform_pump_events(void) {
	SDL_Event ev;
	// キューから次のイベントを取得し、なくなるまで処理を繰り返す
	while (SDL_PollEvent(&ev)) {
		// ALT+Enterでフルスクリーン切り替え
		// -------------------------------------
		// 多くのゲームで使われる標準的なショートカット
		if (
			ev.type == SDL_KEYDOWN && 
			ev.key.keysym.scancode == SDL_SCANCODE_RETURN &&
			(ev.key.keysym.mod & (KMOD_LALT | KMOD_RALT))
		) {
			platform_set_fullscreen(!platform_get_fullscreen());
		}

		// キーボード入力処理
		// -------------------------------------
		else if (ev.type == SDL_KEYDOWN || ev.type == SDL_KEYUP) {
			// スキャンコード（キーボード上の物理的な位置）を取得
			int code = ev.key.keysym.scancode;
			// 押された場合は1.0、離された場合は0.0
			float state = ev.type == SDL_KEYDOWN ? 1.0 : 0.0;
			
			// 修飾キー（Ctrl, Shift, Alt）の処理
			if (code >= SDL_SCANCODE_LCTRL && code <= SDL_SCANCODE_RALT) {
				int code_internal = code - SDL_SCANCODE_LCTRL + INPUT_KEY_L_CTRL;
				input_set_button_state(code_internal, state);
			}
			// その他の通常キーの処理
			else if (code > 0 && code < INPUT_KEY_MAX) {
				input_set_button_state(code, state);
			}
		}

		// テキスト入力処理
		// -------------------------------------
		// キー入力とは別に、テキスト入力として処理（IME対応など）
		else if (ev.type == SDL_TEXTINPUT) {
			input_textinput(ev.text.text[0]);
		}

		// ゲームパッドの接続/切断処理
		// -------------------------------------
		else if (ev.type == SDL_CONTROLLERDEVICEADDED) {
			// 新しいゲームパッドが接続された
			gamepad = SDL_GameControllerOpen(ev.cdevice.which);
		}
		else if (ev.type == SDL_CONTROLLERDEVICEREMOVED) {
			// ゲームパッドが切断された
			if (gamepad && ev.cdevice.which == SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(gamepad))) {
				SDL_GameControllerClose(gamepad);
				// 他に接続されているゲームパッドがあれば使用
				gamepad = platform_find_gamepad();
			}
		}

		// ゲームパッドボタン入力処理
		// -------------------------------------
		else if (
			ev.type == SDL_CONTROLLERBUTTONDOWN || 
			ev.type == SDL_CONTROLLERBUTTONUP
		) {
			if (ev.cbutton.button < SDL_CONTROLLER_BUTTON_MAX) {
				// SDLのボタン定数をエンジン内部の入力定数に変換
				button_t button = platform_sdl_gamepad_map[ev.cbutton.button];
				if (button != INPUT_INVALID) {
					// 押された場合は1.0、離された場合は0.0
					float state = ev.type == SDL_CONTROLLERBUTTONDOWN ? 1.0 : 0.0;
					input_set_button_state(button, state);
				}
			}
		}

		// ゲームパッドアナログ入力処理
		// -------------------------------------
		// スティックやトリガーなどのアナログ入力
		else if (ev.type == SDL_CONTROLLERAXISMOTION) {
			// 値を-1.0〜1.0の範囲に正規化
			float state = (float)ev.caxis.value / 32767.0;

			if (ev.caxis.axis < SDL_CONTROLLER_AXIS_MAX) {
				// SDLの軸定数をエンジン内部の入力定数に変換
				int code = platform_sdl_axis_map[ev.caxis.axis];
				if (
					code == INPUT_GAMEPAD_L_TRIGGER || 
					code == INPUT_GAMEPAD_R_TRIGGER
				) {
					// トリガーは0.0〜1.0の範囲で処理
					input_set_button_state(code, state);
				}
				else if (state > 0) {
					// 正の値（右/下方向）
					input_set_button_state(code, 0.0);      // 左/上を0に
					input_set_button_state(code+1, state);  // 右/下に値を設定
				}
				else {
					// 負の値（左/上方向）
					input_set_button_state(code, -state);   // 左/上に正の値を設定
					input_set_button_state(code+1, 0.0);    // 右/下を0に
				}
			}
		}

		// マウスボタン入力処理
		// -------------------------------------
		else if (
			ev.type == SDL_MOUSEBUTTONDOWN ||
			ev.type == SDL_MOUSEBUTTONUP
		) {
			// SDLのマウスボタン定数をエンジン内部の入力定数に変換
			button_t button = INPUT_BUTTON_NONE;
			switch (ev.button.button) {
				case SDL_BUTTON_LEFT: button = INPUT_MOUSE_LEFT; break;
				case SDL_BUTTON_MIDDLE: button = INPUT_MOUSE_MIDDLE; break;
				case SDL_BUTTON_RIGHT: button = INPUT_MOUSE_RIGHT; break;
				default: break;
			}
			if (button != INPUT_BUTTON_NONE) {
				// 押された場合は1.0、離された場合は0.0
				float state = ev.type == SDL_MOUSEBUTTONDOWN ? 1.0 : 0.0;
				input_set_button_state(button, state);
			}
		}

		// マウスホイール処理
		// -------------------------------------
		else if (ev.type == SDL_MOUSEWHEEL) {
			// ホイールの回転方向に応じてボタンを決定
			button_t button = ev.wheel.y > 0 
				? INPUT_MOUSE_WHEEL_UP    // 上方向
				: INPUT_MOUSE_WHEEL_DOWN; // 下方向
			// ホイールイベントは「クリック」扱いにするため、押して即離す
			input_set_button_state(button, 1.0);
			input_set_button_state(button, 0.0);
		}

		// マウス移動処理
		// -------------------------------------
		else if (ev.type == SDL_MOUSEMOTION) {
			// マウス座標を内部に設定
			input_set_mouse_pos(ev.motion.x, ev.motion.y);
		}

		// ウィンドウイベント処理
		// -------------------------------------
		if (ev.type == SDL_QUIT) {
			// ウィンドウの閉じるボタンが押された
			wants_to_exit = true;
		}
		else if (
			ev.type == SDL_WINDOWEVENT &&
			(
				ev.window.event == SDL_WINDOWEVENT_SIZE_CHANGED ||
				ev.window.event == SDL_WINDOWEVENT_RESIZED
			)
		) {
			// ウィンドウサイズが変更された
			// 新しいサイズに合わせてエンジンの内部状態を更新
			engine_resize(platform_screen_size());
		}
	}
}

// 現在の時刻を秒単位で取得
// -----------------------------------------------------------------------------
// 【初心者向け解説】
// この関数は、高精度のタイマーを使用して現在の時刻を取得します。
// 返される値は「秒」単位ですが、マイクロ秒（百万分の1秒）レベルの
// 精度を持っています。
//
// ゲーム開発では、アニメーションの更新、物理シミュレーション、
// フレームレート計算など、多くの場面で正確な時間測定が必要です。
// 
// SDL_GetPerformanceCounterとperf_freqを使うことで、
// プラットフォームに依存しない高精度の時間計測が可能になります。
double platform_now(void) {
	// 現在のパフォーマンスカウンター値を取得
	uint64_t perf_counter = SDL_GetPerformanceCounter();
	// カウンター値を周波数で割って秒に変換
	return (double)perf_counter / (double)perf_freq;
}

// ウィンドウがフルスクリーンモードかどうかを確認
// -----------------------------------------------------------------------------
// 【初心者向け解説】
// この関数は、現在のウィンドウがフルスクリーンモードで
// 表示されているかどうかをチェックします。
//
// SDLではウィンドウの状態を「フラグ」という形で管理します。
// ビット演算子 & を使って、SDL_WINDOW_FULLSCREENフラグが
// 設定されているかどうかを確認しています。
bool platform_get_fullscreen(void) {
	return SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN;
}

// ウィンドウのフルスクリーンモードを設定
// -----------------------------------------------------------------------------
// 【初心者向け解説】
// この関数は、ウィンドウをフルスクリーンモードとウィンドウモードの
// 間で切り替えます。
//
// フルスクリーンモードでは、ゲームがモニター全体を使用し、
// マウスカーソルは非表示になります。これにより、没入感が高まり、
// パフォーマンスが向上する場合があります。
//
// 一方、ウィンドウモードでは、ゲームは通常のウィンドウとして表示され、
// マウスカーソルは表示されます。他のアプリケーションと簡単に
// 切り替えることができます。
void platform_set_fullscreen(bool fullscreen) {
	if (fullscreen) {
		// フルスクリーンモードに切り替える
		// ウィンドウが表示されているディスプレイのインデックスを取得
		int32_t display = SDL_GetWindowDisplayIndex(window);
		
		// そのディスプレイのネイティブ解像度と更新レートを取得
		SDL_DisplayMode mode;
		SDL_GetDesktopDisplayMode(display, &mode);
		// ウィンドウの表示モードを設定
		SDL_SetWindowDisplayMode(window, &mode);
		// ウィンドウをフルスクリーンに設定
		SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN);
		// マウスカーソルを非表示に
		SDL_ShowCursor(SDL_DISABLE);
	}
	else {
		// ウィンドウモードに戻す
		SDL_SetWindowFullscreen(window, 0);
		// マウスカーソルを表示
		SDL_ShowCursor(SDL_ENABLE);
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
// - 22050 Hz（低品質/低リソース環境向け）
//
// エンジンのサウンドシステムは、このサンプルレートに合わせて
// オーディオデータを生成する必要があります。
uint32_t platform_samplerate(void) {
	return platform_output_samplerate;
}

// SDLオーディオコールバック
// -----------------------------------------------------------------------------
// 【初心者向け解説】
// これはSDLのオーディオシステムから定期的に呼び出される関数です。
// オーディオデバイスがサウンド出力用のデータを必要とするたびに実行されます。
//
// この関数は、エンジンのオーディオミキシングシステム（audio_callback）を
// 呼び出してサウンドデータを生成するか、コールバックが設定されていない場合は
// 無音（0で埋めたバッファ）を出力します。
//
// オーディオプログラミングでは、このようなコールバックベースの設計が一般的です。
// 必要なタイミングでデータを「プル」することで、バッファのアンダーランを防ぎます。
void platform_audio_callback(void* userdata, uint8_t* stream, int len) {
	if (audio_callback) {
		// エンジンのオーディオコールバックが設定されている場合、それを呼び出す
		// streamを浮動小数点数配列として扱い、サンプル数に変換
		audio_callback((float *)stream, len/sizeof(float));
	}
	else {
		// コールバックが設定されていない場合は無音を出力
		memset(stream, 0, len);
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
//
// この関数は、コールバックを設定した後、オーディオデバイスの再生を開始します。
void platform_set_audio_mix_cb(void (*cb)(float *buffer, uint32_t len)) {
	// オーディオコールバック関数を保存
	audio_callback = cb;
	// オーディオデバイスの一時停止を解除（再生開始）
	// 第2引数が0の場合は再生、1の場合は一時停止
	SDL_PauseAudioDevice(audio_device, 0);
}


// アセットファイルの読み込み
// -----------------------------------------------------------------------------
// 【初心者向け解説】
// この関数は、ゲームに必要なアセット（画像、サウンド、マップデータなど）を
// 読み込むために使用されます。アセットは通常、ゲームに付属するリソースで、
// ユーザーが変更することはありません。
//
// 読み込みの手順：
// 1. まずQOPパッケージ（実行ファイルに埋め込まれたアセット）から読み込みを試みる
// 2. QOPパッケージに見つからない場合は、アセットフォルダから個別ファイルを読み込む
//
// QOPパッケージ形式のメリット：
// - ゲーム配布が簡単（1つの実行ファイルだけで済む）
// - ファイルアクセスが効率的（1回のディスクアクセスで複数のファイルを読み込める）
// - ファイルの改ざんが難しくなる
uint8_t *platform_load_asset(const char *name, uint32_t *bytes_read) {
	// まずQOPアーカイブからの読み込みを試みる
	if (qop.index_len) {
		// QOPアーカイブ内でファイルを検索
		qop_file *f = qop_find(&qop, name);
		if (f) {
			// 見つかった場合、一時メモリに読み込む
			uint8_t *data = temp_alloc(f->size);
			*bytes_read = qop_read(&qop, f, data);
			return data;
		}
	}

	// QOPアーカイブにない場合、個別のファイルとして読み込む
	// アセットパスとファイル名を連結して完全なパスを作成
	char *path = strcat(strcpy(temp_path, path_assets), name);
	return file_load(path, bytes_read);
}

// ユーザーデータファイルの読み込み
// -----------------------------------------------------------------------------
// 【初心者向け解説】
// この関数は、ユーザー固有のデータ（セーブデータ、設定など）を読み込むために
// 使用されます。アセットとは異なり、ユーザーデータはゲーム進行中に変更され、
// プレイヤー固有の情報を保存します。
//
// ユーザーデータの格納場所は通常、OSごとに異なります：
// - Windows: "C:\Users\(ユーザー名)\AppData\Roaming\(ゲーム名)"
// - macOS: "/Users/(ユーザー名)/Library/Application Support/(ゲーム名)"
// - Linux: "/home/(ユーザー名)/.local/share/(ゲーム名)"
//
// この違いはSDLのSDL_GetPrefPath関数によって抽象化されています。
uint8_t *platform_load_userdata(const char *name, uint32_t *bytes_read) {
	// ユーザーデータパスとファイル名を連結
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
// この関数は、ユーザー固有のデータ（セーブデータ、設定など）をディスクに
// 保存するために使用されます。プレイヤーの進行状況やゲーム設定を永続化するのに
// 重要です。
//
// ユーザーデータの格納場所は、platform_load_userdata関数と同じで、
// OSごとに適切なディレクトリが使用されます。
uint32_t platform_store_userdata(const char *name, void *bytes, int32_t len) {
	// ユーザーデータパスとファイル名を連結
	char *path = strcat(strcpy(temp_path, path_userdata), name);
	// ファイルを保存し、書き込まれたバイト数を返す
	return file_store(path, bytes, len);
}

//
// OpenGLレンダラーの実装
// =============================================================================
// 【初心者向け解説】
// OpenGLは、クロスプラットフォームの3Dグラフィックスライブラリです。
// 2Dゲームでも、高速な描画とハードウェアアクセラレーションのために使用されます。
//
// OpenGLの主な特徴：
// - 多くのプラットフォームで利用可能（Windows、macOS、Linux、モバイル）
// - 長い歴史があり、広くサポートされている
// - グラフィックスカードの機能を直接利用できる
//
// この部分のコードは、OpenGLを使ったレンダリング用の初期化と設定を行います。
//
#if defined(RENDER_GL) // ------------------------------------------------------
	// SDLウィンドウにOpenGLサポートを追加するフラグ
	#define PLATFORM_WINDOW_FLAGS SDL_WINDOW_OPENGL
	// OpenGLコンテキスト（描画状態を保持するオブジェクト）
	SDL_GLContext platform_gl;

	// ビデオサブシステムの初期化
	// -------------------------------------------------------------------------
	// 【初心者向け解説】
	// この関数は、OpenGLの描画環境を設定します。具体的には：
	// 1. OpenGLのバージョンとプロファイルの設定
	// 2. 描画コンテキストの作成
	// 3. 垂直同期（VSync）の設定
	//
	// コンテキストプロファイルとは、OpenGLの機能セットを指定するもので：
	// - PROFILE_ES: モバイルデバイスやWebブラウザ向けの軽量版
	// - PROFILE_CORE: デスクトップ向けの新しい機能を含むバージョン
	void platform_video_init(void) {
		#if defined(__EMSCRIPTEN__) || defined(USE_GLES2)
			// WebブラウザまたはモバイルデバイスではOpenGL ESを使用
			SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
			SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);  // バージョン2.1を指定
			SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
		#else
			// デスクトップではOpenGL Coreプロファイルを使用
			SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
			SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);  // バージョン3.1を指定 
			SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
		#endif

		// OpenGLコンテキストを作成
		platform_gl = SDL_GL_CreateContext(window);
		// 垂直同期（VSync）を設定
		// これにより、モニターのリフレッシュレートに合わせてフレーム更新が同期され、
		// 画面のちらつきを防ぎます
		SDL_GL_SetSwapInterval(PLATFORM_VSYNC);
	}

	// フレーム描画の準備
	// -------------------------------------------------------------------------
	// 【初心者向け解説】
	// この関数は、新しいフレームの描画を開始する前に呼び出されます。
	// OpenGLの場合、特別な準備は必要ないので空になっています。
	void platform_prepare_frame(void) {
		// OpenGLでは特に何もする必要がない
	}

	// ビデオリソースのクリーンアップ
	// -------------------------------------------------------------------------
	// 【初心者向け解説】
	// この関数は、プログラム終了時にOpenGLリソースを解放します。
	// これにより、メモリリークを防ぎ、クリーンな終了を保証します。
	void platform_video_cleanup(void) {
		// OpenGLコンテキストを破棄
		SDL_GL_DeleteContext(platform_gl);
	}

	// フレーム描画の終了
	// -------------------------------------------------------------------------
	// 【初心者向け解説】
	// この関数は、フレームの描画が完了した後に呼び出されます。
	// OpenGLでは、バックバッファとフロントバッファを交換（スワップ）して、
	// 描画した内容を画面に表示します。
	//
	// ダブルバッファリングとは：
	// 1. バックバッファ（見えない領域）に描画する
	// 2. 描画が完了したらフロントバッファ（表示されている領域）と交換する
	// これにより、描画途中の不完全な画面がユーザーに見えることを防ぎます。
	void platform_end_frame(void) {
		// バックバッファとフロントバッファを交換
		SDL_GL_SwapWindow(window);
	}

	// 画面サイズの取得
	// -------------------------------------------------------------------------
	// 【初心者向け解説】
	// この関数は、現在のウィンドウの描画可能領域のサイズを返します。
	// 高DPI（Retinaディスプレイなど）では、ウィンドウサイズと描画サイズが
	// 異なる場合があります。
	vec2i_t platform_screen_size(void) {
		int width, height;
		// OpenGLの描画領域サイズを取得
		SDL_GL_GetDrawableSize(window, &width, &height);
		// 幅と高さを持つベクトル構造体を返す
		return vec2i(width, height);
	}

//
// Metalレンダラーの実装
// =============================================================================
// 【初心者向け解説】
// MetalはAppleが開発した、macOSとiOS向けの低レベルグラフィックスAPI（プログラミング
// インターフェース）です。OpenGLより新しく、より効率的です。
//
// Metalの主な特徴：
// - Apple製品（MacやiPhone、iPad）に最適化されている
// - 低オーバーヘッドで高速なグラフィックス処理が可能
// - GPU（グラフィックスプロセッシングユニット）と直接やり取りできる
// - OpenGLよりも複雑だが、より多くの制御が可能
//
// この部分のコードは、Apple製デバイスでMetalを使用するための設定を行います。
//
#elif defined(RENDER_METAL) // ----------------------------------------------
	// Metalには特別なウィンドウフラグは必要ない
	#define PLATFORM_WINDOW_FLAGS 0
	// Metal描画用のビュー
	static SDL_MetalView *metal_view;
	// SDLレンダラー（Metalと連携）
	static SDL_Renderer *renderer;
	// CAMetalLayerポインタ（Objective-C++のレイヤーオブジェクト）
	static void *metal_layer;

	// ビデオサブシステムの初期化
	// -------------------------------------------------------------------------
	// 【初心者向け解説】
	// この関数は、Metal描画環境を設定します。具体的には：
	// 1. MetalビューとCAMetalLayerの作成
	// 2. SDLレンダラーの作成（垂直同期付き）
	//
	// MetalはmacOSとiOSでのみ使用可能なため、このコードは
	// Apple製デバイスでのみコンパイルされます。
	void platform_video_init() {
		// SDLのMetalビューを作成
		metal_view = SDL_Metal_CreateView(window);
		// ビューからCAMetalLayerを取得（Metal描画の基礎となるレイヤー）
		metal_layer = SDL_Metal_GetLayer(metal_view);
		// レンダラーを作成（ハードウェアアクセラレーションと垂直同期を有効化）
		renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
	}

	// ビデオリソースのクリーンアップ
	// -------------------------------------------------------------------------
	// 【初心者向け解説】
	// この関数は、プログラム終了時にMetalリソースを解放します。
	// これにより、メモリリークを防ぎ、クリーンな終了を保証します。
	void platform_video_cleanup() {
		// Metalビューを破棄
		SDL_Metal_DestroyView(metal_view);
		metal_view = NULL;
		metal_layer = NULL;
		// レンダラーを破棄
		SDL_DestroyRenderer(renderer);
		renderer = NULL;
	}

	// フレーム描画の準備
	// -------------------------------------------------------------------------
	// 【初心者向け解説】
	// この関数は、新しいフレームの描画を開始する前に呼び出されます。
	// Metalレンダラーの場合、初期化は実際のレンダリングコード（render_metal.m）
	// で行われるため、ここでは何もしません。
	void platform_prepare_frame() {
		// この実装では特に何もする必要がない
	}

	// フレーム描画の終了
	// -------------------------------------------------------------------------
	// 【初心者向け解説】
	// この関数は、フレームの描画が完了した後に呼び出されます。
	// Metalレンダラーの場合、バッファのスワップも実際のレンダリングコード
	// （render_metal.m）で行われるため、ここでは何もしません。
	void platform_end_frame() {
		// この実装では特に何もする必要がない
	}

	// 画面サイズの取得
	// -------------------------------------------------------------------------
	// 【初心者向け解説】
	// この関数は、現在のウィンドウの描画可能領域のサイズを返します。
	// 高DPI（Retinaディスプレイなど）では、ウィンドウサイズと描画サイズが
	// 異なる場合があります。
	vec2i_t platform_screen_size() {
		int width, height;
		// Metalの描画領域サイズを取得
		SDL_Metal_GetDrawableSize(window, &width, &height);
		// 幅と高さを持つベクトル構造体を返す
		vec2i_t screen_size = vec2i(width, height);
		return screen_size;
	}

	// MetalレイヤーのポインタをMetalレンダラーに提供
	// -------------------------------------------------------------------------
	// 【初心者向け解説】
	// この関数は、CAMetalLayerポインタをMetalレンダラー（render_metal.m）に
	// 提供します。これにより、レンダラーは正しいレイヤーに描画できます。
	void *platform_get_metal_layer() {
		return metal_layer;
	}

//
// ソフトウェアレンダラーの実装
// =============================================================================
// 【初心者向け解説】
// ソフトウェアレンダリングは、GPUを使わずにCPUでグラフィックスを処理する方法です。
// 通常はOpenGLやMetalのようなハードウェアレンダリングより遅いですが、
// どんなデバイスでも動作し、シンプルで理解しやすいという利点があります。
//
// ソフトウェアレンダリングの特徴：
// - すべてCPUで計算するため、古いハードウェアでも動作する
// - ピクセル単位の操作が完全に制御可能
// - デバッグが容易で、視覚的なバグを追跡しやすい
// - GPU依存のバグがない
// - ただし、一般的に処理速度は遅い
//
// この部分のコードは、CPUで描画したピクセルデータをSDLテクスチャに転送して
// 表示するための仕組みを提供します。
//
#elif defined(RENDER_SOFTWARE) // ----------------------------------------------
	// ソフトウェアレンダリングには特別なウィンドウフラグは必要ない
	#define PLATFORM_WINDOW_FLAGS 0
	// SDLレンダラー
	static SDL_Renderer *renderer;
	// 画面バッファとして使用するSDLテクスチャ
	static SDL_Texture *screenbuffer = NULL;
	// テクスチャのピクセルデータへのポインタ
	static void *screenbuffer_pixels = NULL;
	// 1行のバイト数（ピッチ）
	static int screenbuffer_pitch;
	// スクリーンバッファのサイズ
	static vec2i_t screenbuffer_size = vec2i(0, 0);
	// 画面の実際のサイズ
	static vec2i_t screen_size = vec2i(0, 0);


	// ビデオサブシステムの初期化
	// -------------------------------------------------------------------------
	// 【初心者向け解説】
	// この関数は、ソフトウェアレンダリングの描画環境を設定します。
	// SDLレンダラーを作成し、VSyncの設定を行います。
	//
	// ソフトウェアレンダリングでも、テクスチャからスクリーンへの転送は
	// ハードウェアアクセラレーションを使用するため、SDL_RENDERER_ACCELERATED
	// フラグを使用しています。
	void platform_video_init(void) {
		// レンダラーを作成（ハードウェアアクセラレーションと必要に応じて垂直同期を有効化）
		renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | (PLATFORM_VSYNC ? SDL_RENDERER_PRESENTVSYNC : 0));
		// 垂直同期（VSync）を設定
		SDL_GL_SetSwapInterval(PLATFORM_VSYNC);
	}

	// ビデオリソースのクリーンアップ
	// -------------------------------------------------------------------------
	// 【初心者向け解説】
	// この関数は、プログラム終了時にビデオリソースを解放します。
	// スクリーンバッファとレンダラーを破棄します。
	void platform_video_cleanup(void) {
		// スクリーンバッファが存在する場合は破棄
		if (screenbuffer) {
			SDL_DestroyTexture(screenbuffer);
		}
		// レンダラーを破棄
		SDL_DestroyRenderer(renderer);
	}

	// フレーム描画の準備
	// -------------------------------------------------------------------------
	// 【初心者向け解説】
	// この関数は、新しいフレームの描画を開始する前に呼び出されます。
	// 主に2つの役割があります：
	// 1. ウィンドウサイズが変更された場合、新しいサイズのテクスチャを作成
	// 2. テクスチャをロックして、ピクセルデータに直接書き込めるようにする
	void platform_prepare_frame(void) {
		// 画面サイズが変わった場合は新しいテクスチャを作成
		if (screen_size.x != screenbuffer_size.x || screen_size.y != screenbuffer_size.y) {
			// 古いテクスチャがあれば破棄
			if (screenbuffer) {
				SDL_DestroyTexture(screenbuffer);
			}
			// 新しいテクスチャを作成（ABGR形式、ストリーミングアクセス用）
			screenbuffer = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING, screen_size.x, screen_size.y);
			screenbuffer_size = screen_size;
		}
		// テクスチャをロックして直接書き込み可能にする
		// screenbuffer_pixelsにピクセルデータへのポインタが設定される
		// screenbuffer_pitchに1行のバイト数が設定される
		SDL_LockTexture(screenbuffer, NULL, &screenbuffer_pixels, &screenbuffer_pitch);
	}

	// フレーム描画の終了
	// -------------------------------------------------------------------------
	// 【初心者向け解説】
	// この関数は、フレームの描画が完了した後に呼び出されます。
	// 主に3つの役割があります：
	// 1. テクスチャのロックを解除する
	// 2. テクスチャの内容をレンダラーにコピーする
	// 3. レンダラーの内容を画面に表示する
	void platform_end_frame(void) {
		// ピクセルデータへの参照をクリア
		screenbuffer_pixels = NULL;
		// テクスチャのロックを解除（この時点でピクセルデータがテクスチャに反映される）
		SDL_UnlockTexture(screenbuffer);
		// テクスチャをレンダラーにコピー
		SDL_RenderCopy(renderer, screenbuffer, NULL, NULL);
		// レンダラーの内容を画面に表示
		SDL_RenderPresent(renderer);
	}

	// スクリーンバッファへのアクセスを提供
	// -------------------------------------------------------------------------
	// 【初心者向け解説】
	// この関数は、ソフトウェアレンダラーに画面のピクセルデータへの直接アクセスを
	// 提供します。レンダラーはこのバッファに描画し、フレーム終了時に画面に表示されます。
	//
	// ピッチ（pitch）は、画像データの1行あたりのバイト数です。これは、
	// メモリ内の次の行の開始位置を計算するために使用されます。
	rgba_t *platform_get_screenbuffer(int32_t *pitch) {
		// ピッチ（1行あたりのバイト数）を設定
		*pitch = screenbuffer_pitch;
		// ピクセルデータへのポインタを返す
		return screenbuffer_pixels;
	}

	// 画面サイズの取得
	// -------------------------------------------------------------------------
	// 【初心者向け解説】
	// この関数は、現在のウィンドウサイズを返します。
	// ソフトウェアレンダリングでは、ウィンドウサイズがそのまま描画サイズになります。
	vec2i_t platform_screen_size(void) {
		int width, height;
		// ウィンドウサイズを取得
		SDL_GetWindowSize(window, &width, &height);
		// サイズをキャッシュ
		screen_size = vec2i(width, height);
		return screen_size;
	}

#else
	// サポートされていないレンダラーの場合はコンパイルエラー
	#error "Unsupported renderer for platform SDL"
#endif

// メイン関数 - プログラムのエントリーポイント
// =============================================================================
// 【初心者向け解説】
// メイン関数はプログラムの実行開始点です。ここでは次のことを行います：
// 1. SDLの初期化
// 2. ファイルパスの設定
// 3. リソースの読み込み
// 4. ウィンドウとレンダラーの作成
// 5. ゲームループの実行
// 6. 終了時のクリーンアップ
//
// このファイルがコンパイルされると、SDLを使った実行可能なゲームプログラムが
// 作成されます。
int main(int argc, char *argv[]) {
	// SDLの初期化
	// -------------------------------------------------------------------------
	// 必要なサブシステム（ビデオ、オーディオ、ジョイスティック、ゲームコントローラー）を初期化
	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER);

	// アセットとユーザーデータのパスを決定
	// -------------------------------------------------------------------------
	// 【初心者向け解説】
	// アセットパス: ゲームに付属する読み取り専用のリソースの場所
	// ユーザーデータパス: プレイヤー固有の保存データの場所
	//
	// これらのパスは、コンパイル時に定義するか、実行時にSDLから取得します。
	// SDLは各OSに適した標準的な場所を提供します。

	// アセットパスの設定
	char *sdl_path_assets = NULL;
	#ifdef PATH_ASSETS
		// コンパイル時に定義されている場合はそれを使用
		path_assets = TOSTRING(PATH_ASSETS);
	#else
		// 定義されていない場合はSDLから取得
		sdl_path_assets = SDL_GetBasePath();
		if (sdl_path_assets) {
			path_assets = sdl_path_assets;
		}
		// SDLが失敗した場合はカレントディレクトリを使用（空文字列）
	#endif

	// ユーザーデータパスの設定
	char *sdl_path_userdata = NULL;
	#ifdef PATH_USERDATA
		// コンパイル時に定義されている場合はそれを使用
		path_userdata = TOSTRING(PATH_USERDATA);
	#else
		// 定義されていない場合はSDLから取得
		sdl_path_userdata = SDL_GetPrefPath(GAME_VENDOR, GAME_NAME);
		if (sdl_path_userdata) {
			path_userdata = sdl_path_userdata;
		}
		// SDLが失敗した場合はカレントディレクトリを使用（空文字列）
	#endif

	// 一時パスバッファの確保
	// -------------------------------------------------------------------------
	// アセットパスとファイル名を連結するための一時バッファを確保
	temp_path = bump_alloc(max(strlen(path_assets), strlen(path_userdata)) + PLATFORM_MAX_PATH);


	// QOPアーカイブのロード（可能な場合）
	// -------------------------------------------------------------------------
	// 【初心者向け解説】
	// リリースビルドでは、実行可能ファイルにアセットを埋め込むことができます。
	// これにより、ゲームを1つのファイルとして配布できます。
	char *exe_path = platform_executable_path();
	if (exe_path && qop_open(exe_path, &qop)) {
		printf("Opened QOP archive from %s; %d bytes, %d files\n", exe_path, qop.files_offset, qop.index_len);
		// QOPアーカイブのインデックスを読み込む
		qop_read_index(&qop, bump_alloc(qop.hashmap_size));
	}

	// ゲームコントローラーデータベースの読み込み
	// -------------------------------------------------------------------------
	// 【初心者向け解説】
	// このファイルには、様々なゲームパッドのボタン配置情報が含まれています。
	// これにより、異なるメーカーのコントローラーでも同じように動作します。
	char *gcdb_path = strcat(strcpy(temp_path, path_assets), "gamecontrollerdb.txt");
	int gcdb_res = SDL_GameControllerAddMappingsFromFile(gcdb_path);
	if (gcdb_res < 0) {
		printf("Failed to load gamecontrollerdb.txt\n");
	}
	else {
		printf("Loaded gamecontrollerdb.txt\n");
	}


	// 接続されているゲームパッドを検索して開く
	gamepad = platform_find_gamepad();

	// パフォーマンスカウンターの周波数を取得（時間計測用）
	perf_freq = SDL_GetPerformanceFrequency();

	// オーディオデバイスを開く
	// -------------------------------------------------------------------------
	// 【初心者向け解説】
	// オーディオ設定：
	// - サンプルレート：44.1kHz（CD品質）
	// - フォーマット：32ビット浮動小数点（高品質）
	// - チャンネル：2（ステレオ）
	// - バッファサイズ：1024サンプル（レイテンシとCPU使用率のバランス）
	SDL_AudioSpec obtained_spec;
	audio_device = SDL_OpenAudioDevice(NULL, 0, &(SDL_AudioSpec){
		.freq = platform_output_samplerate,  // サンプルレート
		.format = AUDIO_F32SYS,              // 32ビット浮動小数点フォーマット
		.channels = 2,                       // ステレオ
		.samples = 1024,                     // バッファサイズ
		.callback = platform_audio_callback  // オーディオコールバック関数
	}, &obtained_spec, 0);


	// ウィンドウを作成
	// -------------------------------------------------------------------------
	// 【初心者向け解説】
	// ウィンドウ設定：
	// - タイトル：ゲームタイトル
	// - 位置：画面中央
	// - サイズ：設定された幅と高さ
	// - フラグ：表示可能、リサイズ可能、レンダラー固有のフラグ、高DPIサポート
	window = SDL_CreateWindow(
		WINDOW_TITLE,                        // ウィンドウタイトル
		SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,  // 画面中央に配置
		WINDOW_WIDTH, WINDOW_HEIGHT,         // ウィンドウの初期サイズ
		SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | PLATFORM_WINDOW_FLAGS | SDL_WINDOW_ALLOW_HIGHDPI
	);

	// 選択されたレンダラー（OpenGL、Metal、ソフトウェア）を初期化
	platform_video_init();

	// オーディオデバイスの実際のサンプルレートを取得（要求と異なる場合がある）
	platform_output_samplerate = obtained_spec.freq;
	
	// ゲームエンジンを初期化
	engine_init();
	
	// メインゲームループ
	// -------------------------------------------------------------------------
	// 【初心者向け解説】
	// ゲームループは、ゲームが実行されている間、繰り返し実行される中心部分です。
	// 各繰り返し（フレーム）で、以下の処理を行います：
	// 1. イベント処理（ユーザー入力など）
	// 2. フレーム描画の準備
	// 3. ゲームロジックの更新
	// 4. フレームの描画と表示
	//
	// wants_to_exitフラグがtrueになると（ウィンドウの閉じるボタンを押すなど）、
	// ループを抜けてプログラムが終了します。
	while (!wants_to_exit) {
		platform_pump_events();     // イベント（入力など）の処理
		platform_prepare_frame();   // フレーム描画の準備
		engine_update();            // ゲームロジックの更新
		platform_end_frame();       // 画面への描画完了と表示
	}

	// クリーンアップ処理
	// -------------------------------------------------------------------------
	// 【初心者向け解説】
	// プログラム終了時に、確保したすべてのリソースを解放します。
	// これにより、メモリリークを防ぎ、OSに正しくリソースを返却します。
	
	// ゲームエンジンのクリーンアップ
	engine_cleanup();
	// ビデオサブシステムのクリーンアップ
	platform_video_cleanup();

	// QOPアーカイブを閉じる（使用していた場合）
	if (qop.index_len) {
		qop_close(&qop);
	}

	// ウィンドウを破棄
	SDL_DestroyWindow(window);

	// ゲームパッドを閉じる
	if (gamepad) {
		SDL_GameControllerClose(gamepad);
	}

	// SDLが提供したパスを解放
	if (sdl_path_assets) {
		SDL_free(sdl_path_assets);
	}
	if (sdl_path_userdata) {
		SDL_free(sdl_path_userdata);
	}

	// オーディオデバイスを閉じる
	SDL_CloseAudioDevice(audio_device);
	// SDLを終了
	SDL_Quit();
	return 0;  // 正常終了
}
