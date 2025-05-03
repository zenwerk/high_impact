#ifndef HI_INPUT_H
#define HI_INPUT_H

// 入力システム
// このシステムは異なる入力デバイスからのハンドリングを抽象化してアクションに変換します。
// ゲーム内の同じアクションに対して、1つ以上のキーやボタンを「バインド」できます。
// 【C言語テクニック】抽象化レイヤーを使って、低レベルの入力を高レベルのアクションに
// マッピングしています。これにより、入力デバイスの詳細を気にせずにゲームロジックを
// 実装できます。

#include "types.h"

// デッドゾーン - 正規化された0〜1の範囲内で、ボタン押下が無視される範囲。
// これはゲームコントローラーのスティックなどの「アナログ」入力にのみ影響します。
// 【C言語テクニック】条件付きコンパイルによるデフォルト値の設定
#if !defined(INPUT_DEADZONE)
	#define INPUT_DEADZONE 0.1  // デフォルトのデッドゾーン値
#endif

// input_capture()用のデッドゾーン
// キャプチャモードではより大きい値を使用（誤検出防止）
#if !defined(INPUT_DEADZONE_CAPTURE)
	#define INPUT_DEADZONE_CAPTURE 0.5
#endif

// 離散アクションの最大数
// ゲーム内で定義できるアクションの上限
#if !defined(INPUT_ACTION_MAX)
	#define INPUT_ACTION_MAX 32
#endif

// 特殊な定数
#define INPUT_ACTION_NONE 255  // アクションなし（無効値）
#define INPUT_BUTTON_NONE 0    // ボタンなし（無効値）

// input_bind()用のキーとボタンの名前
// 【C言語テクニック】入力デバイスを統一された列挙型で表現
// 値の間隔が空いているのはプラットフォーム固有のキーコードとの互換性のため
typedef enum {
	INPUT_INVALID = 0,      // 無効な入力
	INPUT_KEY_A = 4,        // キーボード A
	INPUT_KEY_B = 5,
	INPUT_KEY_C = 6,
	INPUT_KEY_D = 7,
	INPUT_KEY_E = 8,
	INPUT_KEY_F = 9,
	INPUT_KEY_G = 10,
	INPUT_KEY_H = 11,
	INPUT_KEY_I = 12,
	INPUT_KEY_J = 13,
	INPUT_KEY_K = 14,
	INPUT_KEY_L = 15,
	INPUT_KEY_M = 16,
	INPUT_KEY_N = 17,
	INPUT_KEY_O = 18,
	INPUT_KEY_P = 19,
	INPUT_KEY_Q = 20,
	INPUT_KEY_R = 21,
	INPUT_KEY_S = 22,
	INPUT_KEY_T = 23,
	INPUT_KEY_U = 24,
	INPUT_KEY_V = 25,
	INPUT_KEY_W = 26,
	INPUT_KEY_X = 27,
	INPUT_KEY_Y = 28,
	INPUT_KEY_Z = 29,
	INPUT_KEY_1 = 30,
	INPUT_KEY_2 = 31,
	INPUT_KEY_3 = 32,
	INPUT_KEY_4 = 33,
	INPUT_KEY_5 = 34,
	INPUT_KEY_6 = 35,
	INPUT_KEY_7 = 36,
	INPUT_KEY_8 = 37,
	INPUT_KEY_9 = 38,
	INPUT_KEY_0 = 39,
	INPUT_KEY_RETURN = 40,
	INPUT_KEY_ESCAPE = 41,
	INPUT_KEY_BACKSPACE = 42,
	INPUT_KEY_TAB = 43,
	INPUT_KEY_SPACE = 44,
	INPUT_KEY_MINUS = 45,
	INPUT_KEY_EQUALS = 46,
	INPUT_KEY_LEFTBRACKET = 47,
	INPUT_KEY_RIGHTBRACKET = 48,
	INPUT_KEY_BACKSLASH = 49,
	INPUT_KEY_HASH = 50,
	INPUT_KEY_SEMICOLON = 51,
	INPUT_KEY_APOSTROPHE = 52,
	INPUT_KEY_TILDE = 53,
	INPUT_KEY_COMMA = 54,
	INPUT_KEY_PERIOD = 55,
	INPUT_KEY_SLASH = 56,
	INPUT_KEY_CAPSLOCK = 57,
	INPUT_KEY_F1 = 58,
	INPUT_KEY_F2 = 59,
	INPUT_KEY_F3 = 60,
	INPUT_KEY_F4 = 61,
	INPUT_KEY_F5 = 62,
	INPUT_KEY_F6 = 63,
	INPUT_KEY_F7 = 64,
	INPUT_KEY_F8 = 65,
	INPUT_KEY_F9 = 66,
	INPUT_KEY_F10 = 67,
	INPUT_KEY_F11 = 68,
	INPUT_KEY_F12 = 69,
	INPUT_KEY_PRINTSCREEN = 70,
	INPUT_KEY_SCROLLLOCK = 71,
	INPUT_KEY_PAUSE = 72,
	INPUT_KEY_INSERT = 73,
	INPUT_KEY_HOME = 74,
	INPUT_KEY_PAGEUP = 75,
	INPUT_KEY_DELETE = 76,
	INPUT_KEY_END = 77,
	INPUT_KEY_PAGEDOWN = 78,
	INPUT_KEY_RIGHT = 79,
	INPUT_KEY_LEFT = 80,
	INPUT_KEY_DOWN = 81,
	INPUT_KEY_UP = 82,
	INPUT_KEY_NUMLOCK = 83,
	INPUT_KEY_KP_DIVIDE = 84,
	INPUT_KEY_KP_MULTIPLY = 85,
	INPUT_KEY_KP_MINUS = 86,
	INPUT_KEY_KP_PLUS = 87,
	INPUT_KEY_KP_ENTER = 88,
	INPUT_KEY_KP_1 = 89,
	INPUT_KEY_KP_2 = 90,
	INPUT_KEY_KP_3 = 91,
	INPUT_KEY_KP_4 = 92,
	INPUT_KEY_KP_5 = 93,
	INPUT_KEY_KP_6 = 94,
	INPUT_KEY_KP_7 = 95,
	INPUT_KEY_KP_8 = 96,
	INPUT_KEY_KP_9 = 97,
	INPUT_KEY_KP_0 = 98,
	INPUT_KEY_KP_PERIOD = 99,

	INPUT_KEY_L_CTRL = 100,
	INPUT_KEY_L_SHIFT = 101,
	INPUT_KEY_L_ALT = 102,
	INPUT_KEY_L_GUI = 103,
	INPUT_KEY_R_CTRL = 104,
	INPUT_KEY_R_SHIFT = 105,
	INPUT_KEY_R_ALT = 106,

	INPUT_KEY_MAX = 107,         // キーボード入力の最大値

	// ゲームパッド入力
	INPUT_GAMEPAD_A = 108,         // ゲームパッドAボタン
	INPUT_GAMEPAD_Y = 109,         // ゲームパッドYボタン
	INPUT_GAMEPAD_B = 110,         // ゲームパッドBボタン
	INPUT_GAMEPAD_X = 111,         // ゲームパッドXボタン
	INPUT_GAMEPAD_L_SHOULDER = 112,// 左ショルダーボタン
	INPUT_GAMEPAD_R_SHOULDER = 113,// 右ショルダーボタン
	INPUT_GAMEPAD_L_TRIGGER = 114, // 左トリガー
	INPUT_GAMEPAD_R_TRIGGER = 115, // 右トリガー
	INPUT_GAMEPAD_SELECT = 116,    // セレクトボタン
	INPUT_GAMEPAD_START = 117,     // スタートボタン
	INPUT_GAMEPAD_L_STICK_PRESS = 118, // 左スティック押し込み
	INPUT_GAMEPAD_R_STICK_PRESS = 119, // 右スティック押し込み
	INPUT_GAMEPAD_DPAD_UP = 120,   // 十字キー上
	INPUT_GAMEPAD_DPAD_DOWN = 121, // 十字キー下
	INPUT_GAMEPAD_DPAD_LEFT = 122, // 十字キー左
	INPUT_GAMEPAD_DPAD_RIGHT = 123,// 十字キー右
	INPUT_GAMEPAD_HOME = 124,      // ホームボタン
	INPUT_GAMEPAD_L_STICK_UP = 125,    // 左スティック上
	INPUT_GAMEPAD_L_STICK_DOWN = 126,  // 左スティック下
	INPUT_GAMEPAD_L_STICK_LEFT = 127,  // 左スティック左
	INPUT_GAMEPAD_L_STICK_RIGHT = 128, // 左スティック右
	INPUT_GAMEPAD_R_STICK_UP = 129,    // 右スティック上
	INPUT_GAMEPAD_R_STICK_DOWN = 130,  // 右スティック下
	INPUT_GAMEPAD_R_STICK_LEFT = 131,  // 右スティック左
	INPUT_GAMEPAD_R_STICK_RIGHT = 132, // 右スティック右

	// マウス入力
	INPUT_MOUSE_LEFT = 134,        // マウス左ボタン
	INPUT_MOUSE_MIDDLE = 135,      // マウス中ボタン
	INPUT_MOUSE_RIGHT = 136,       // マウス右ボタン
	INPUT_MOUSE_WHEEL_UP = 137,    // マウスホイール上
	INPUT_MOUSE_WHEEL_DOWN = 138,  // マウスホイール下

	INPUT_BUTTON_MAX = 139         // 入力の最大数
} button_t;


// キー/ボタンをアクションにバインドする
// 複数のボタンを同じアクションにバインドできますが、1つのキー/ボタンは
// 1つのアクションにしかバインドできません。アクションは単なるuint8_t識別子で、
// 通常はゲーム内で定義した列挙型から値を使用します。
// 【C言語テクニック】引数としてenumとuint8_tを混在させる柔軟性
void input_bind(button_t button, uint8_t action);

// キー/ボタンのバインドを解除する
void input_unbind(button_t button);

// すべてのキー/ボタンのバインドを解除する
void input_unbind_all(void);

// キー/ボタンに現在バインドされているアクションを返す
// ボタンがバインドされていない場合はINPUT_ACTION_NONEを返す
uint8_t input_action_for_button(button_t button);

// 名前からbutton_t列挙型を返す
// すべての可能な名前はinput.cを参照
// JSONコンフィグファイルなどからの読み込み時に便利
// 【C言語テクニック】文字列と列挙型の相互変換
button_t input_name_to_button(const char *name);

// ボタンの名前を返す
const char *input_button_to_name(button_t button);

// アクションの現在の状態を返す
// 離散的なボタンやキーボードキーの場合、0または1
// アナログ入力の場合、INPUT_DEADZONE〜1の範囲の値
// 【C言語テクニック】様々な入力デバイスを統一的なインターフェースで扱う
float input_state(uint8_t action);

// このフレームの直前にそのアクションのボタンが押されたかどうか
bool input_pressed(uint8_t action);

// このフレームの直前にそのアクションのボタンが離されたかどうか
bool input_released(uint8_t action);

// 現在のマウス位置（実ピクセル単位）
vec2_t input_mouse_pos(void);

// すべてのキーとボタンの押下を受け取るキャプチャコールバックを設定する
// テキスト以外の入力の場合、ascii_charは0
// input_capture(NULL, NULL)を呼び出してコールバックを解除
// 【C言語テクニック】関数ポインタを使ったコールバックメカニズム
typedef void(*input_capture_callback_t)
	(void *user, button_t button, int32_t ascii_char);
void input_capture(input_capture_callback_t cb, void *user);


// プラットフォームによって呼び出される関数
// 【C言語テクニック】プラットフォーム抽象化レイヤー
// 以下の関数は通常、ゲームコードから直接呼び出さないでください。
// これらはプラットフォーム層（SDL、Sokolなど）から呼び出されるためのものです。

// 入力システムの初期化
void input_init(void);

// 入力システムのクリーンアップ
void input_cleanup(void);

// 入力状態のクリア（フレーム間で呼び出される）
void input_clear(void);

// ボタンの状態を設定（プラットフォームからの生の入力）
// stateは通常0（押されていない）または1（押されている）ですが、
// アナログスティックなどの場合は0〜1の範囲の値になります
void input_set_button_state(button_t button, float state);

// マウス位置を設定
void input_set_mouse_pos(int32_t x, int32_t y);

// テキスト入力ハンドリング（キーボードからの文字入力）
void input_textinput(int32_t ascii_char);

#endif
