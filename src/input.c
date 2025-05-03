#include <string.h>

#include "input.h"
#include "utils.h"

// ボタン名の配列
// 【C言語テクニック】指定インデックスによる配列初期化
// この方法では、列挙型の値をインデックスとして直接使用し、
// 対応する文字列を配列に格納できます
static const char *button_names[] = {
	NULL,
	NULL,
	NULL,
	NULL,
	[INPUT_KEY_A] = "a",
	[INPUT_KEY_B] = "b",
	[INPUT_KEY_C] = "c",
	[INPUT_KEY_D] = "d",
	[INPUT_KEY_E] = "e",
	[INPUT_KEY_F] = "f",
	[INPUT_KEY_G] = "g",
	[INPUT_KEY_H] = "h",
	[INPUT_KEY_I] = "i",
	[INPUT_KEY_J] = "j",
	[INPUT_KEY_K] = "k",
	[INPUT_KEY_L] = "l",
	[INPUT_KEY_M] = "m",
	[INPUT_KEY_N] = "n",
	[INPUT_KEY_O] = "o",
	[INPUT_KEY_P] = "p",
	[INPUT_KEY_Q] = "q",
	[INPUT_KEY_R] = "r",
	[INPUT_KEY_S] = "s",
	[INPUT_KEY_T] = "t",
	[INPUT_KEY_U] = "u",
	[INPUT_KEY_V] = "v",
	[INPUT_KEY_W] = "w",
	[INPUT_KEY_X] = "x",
	[INPUT_KEY_Y] = "y",
	[INPUT_KEY_Z] = "z",
	[INPUT_KEY_1] = "1",
	[INPUT_KEY_2] = "2",
	[INPUT_KEY_3] = "3",
	[INPUT_KEY_4] = "4",
	[INPUT_KEY_5] = "5",
	[INPUT_KEY_6] = "6",
	[INPUT_KEY_7] = "7",
	[INPUT_KEY_8] = "8",
	[INPUT_KEY_9] = "9",
	[INPUT_KEY_0] = "0",
	[INPUT_KEY_RETURN] = "return",
	[INPUT_KEY_ESCAPE] = "escape",
	[INPUT_KEY_BACKSPACE] = "backspace",
	[INPUT_KEY_TAB] = "tab",
	[INPUT_KEY_SPACE] = "space",
	[INPUT_KEY_MINUS] = "minus",
	[INPUT_KEY_EQUALS] = "equals",
	[INPUT_KEY_LEFTBRACKET] = "l_bracket",
	[INPUT_KEY_RIGHTBRACKET] = "r_bracket",
	[INPUT_KEY_BACKSLASH] = "backslash",
	[INPUT_KEY_HASH] = "hash",
	[INPUT_KEY_SEMICOLON] = "semicolon",
	[INPUT_KEY_APOSTROPHE] = "apostrophe",
	[INPUT_KEY_TILDE] = "tilde",
	[INPUT_KEY_COMMA] = "comma",
	[INPUT_KEY_PERIOD] = "period",
	[INPUT_KEY_SLASH] = "slash",
	[INPUT_KEY_CAPSLOCK] = "capslock",
	[INPUT_KEY_F1] = "f1",
	[INPUT_KEY_F2] = "f2",
	[INPUT_KEY_F3] = "f3",
	[INPUT_KEY_F4] = "f4",
	[INPUT_KEY_F5] = "f5",
	[INPUT_KEY_F6] = "f6",
	[INPUT_KEY_F7] = "f7",
	[INPUT_KEY_F8] = "f8",
	[INPUT_KEY_F9] = "f9",
	[INPUT_KEY_F10] = "f10",
	[INPUT_KEY_F11] = "f11",
	[INPUT_KEY_F12] = "f12",
	[INPUT_KEY_PRINTSCREEN] = "printscreen",
	[INPUT_KEY_SCROLLLOCK] = "scrolllock",
	[INPUT_KEY_PAUSE] = "pause",
	[INPUT_KEY_INSERT] = "insert",
	[INPUT_KEY_HOME] = "home",
	[INPUT_KEY_PAGEUP] = "page_up",
	[INPUT_KEY_DELETE] = "delete",
	[INPUT_KEY_END] = "end",
	[INPUT_KEY_PAGEDOWN] = "page_down",
	[INPUT_KEY_RIGHT] = "right",
	[INPUT_KEY_LEFT] = "left",
	[INPUT_KEY_DOWN] = "down",
	[INPUT_KEY_UP] = "up",
	[INPUT_KEY_NUMLOCK] = "numlock",
	[INPUT_KEY_KP_DIVIDE] = "kp_pdivide",
	[INPUT_KEY_KP_MULTIPLY] = "kp_multiply",
	[INPUT_KEY_KP_MINUS] = "kp_minus",
	[INPUT_KEY_KP_PLUS] = "kp_plus",
	[INPUT_KEY_KP_ENTER] = "kp_enter",
	[INPUT_KEY_KP_1] = "kp_1",
	[INPUT_KEY_KP_2] = "kp_2",
	[INPUT_KEY_KP_3] = "kp_3",
	[INPUT_KEY_KP_4] = "kp_4",
	[INPUT_KEY_KP_5] = "kp_5",
	[INPUT_KEY_KP_6] = "kp_6",
	[INPUT_KEY_KP_7] = "kp_7",
	[INPUT_KEY_KP_8] = "kp_8",
	[INPUT_KEY_KP_9] = "kp_9",
	[INPUT_KEY_KP_0] = "kp_0",
	[INPUT_KEY_KP_PERIOD] = "kp_period",

	[INPUT_KEY_L_CTRL] = "l_ctrl",
	[INPUT_KEY_L_SHIFT] = "l_shift",
	[INPUT_KEY_L_ALT] = "l_alt",
	[INPUT_KEY_L_GUI] = "l_gui",
	[INPUT_KEY_R_CTRL] = "r_ctrl",
	[INPUT_KEY_R_SHIFT] = "r_shift",
	[INPUT_KEY_R_ALT] = "r_alt",
	NULL,
	[INPUT_GAMEPAD_A] = "gamepad_a",
	[INPUT_GAMEPAD_Y] = "gamepad_y",
	[INPUT_GAMEPAD_B] = "gamepad_b",
	[INPUT_GAMEPAD_X] = "gamepad_x",
	[INPUT_GAMEPAD_L_SHOULDER] = "gamepad_l_shoulder",
	[INPUT_GAMEPAD_R_SHOULDER] = "gamepad_r_shoulder",
	[INPUT_GAMEPAD_L_TRIGGER] = "gamepad_l_trigger",
	[INPUT_GAMEPAD_R_TRIGGER] = "gamepad_r_trigger",
	[INPUT_GAMEPAD_SELECT] = "gamepad_select",
	[INPUT_GAMEPAD_START] = "gamepad_start",
	[INPUT_GAMEPAD_L_STICK_PRESS] = "gamepad_l_stick",
	[INPUT_GAMEPAD_R_STICK_PRESS] = "gamepad_r_stick",
	[INPUT_GAMEPAD_DPAD_UP] = "gamepad_dp_up",
	[INPUT_GAMEPAD_DPAD_DOWN] = "gamepad_dp_down",
	[INPUT_GAMEPAD_DPAD_LEFT] = "gamepad_dp_left",
	[INPUT_GAMEPAD_DPAD_RIGHT] = "gamepad_dp_right",
	[INPUT_GAMEPAD_HOME] = "gamepad_home",
	[INPUT_GAMEPAD_L_STICK_UP] = "gamepad_l_stick_up",
	[INPUT_GAMEPAD_L_STICK_DOWN] = "gamepad_l_stick_down",
	[INPUT_GAMEPAD_L_STICK_LEFT] = "gamepad_l_stick_left",
	[INPUT_GAMEPAD_L_STICK_RIGHT] = "gamepad_l_stick_right",
	[INPUT_GAMEPAD_R_STICK_UP] = "gamepad_r_stick_up",
	[INPUT_GAMEPAD_R_STICK_DOWN] = "gamepad_r_stick_down",
	[INPUT_GAMEPAD_R_STICK_LEFT] = "gamepad_r_stick_left",
	[INPUT_GAMEPAD_R_STICK_RIGHT] = "gamepad_r_stick_right",
	NULL,
	[INPUT_MOUSE_LEFT] = "mouse_left",
	[INPUT_MOUSE_MIDDLE] = "mouse_middle",
	[INPUT_MOUSE_RIGHT] = "mouse_right",
	[INPUT_MOUSE_WHEEL_UP] = "mouse_wheel_up",
	[INPUT_MOUSE_WHEEL_DOWN] = "mouse_wheel_wdown",
};

// アクション状態を保持する配列
// 各アクションの現在の状態（0〜1の値）を格納
static float actions_state[INPUT_ACTION_MAX];

// 押された/離されたフラグ配列
// このフレームでアクションが押された/離されたかを追跡
static bool actions_pressed[INPUT_ACTION_MAX];
static bool actions_released[INPUT_ACTION_MAX];

// 期待されるボタン配列（複数ボタンが同じアクションに割り当てられている場合に使用）
static uint8_t expected_button[INPUT_ACTION_MAX];

// ボタンからアクションへのマッピング配列
// 各ボタンに割り当てられたアクションIDを格納
static uint8_t bindings[INPUT_BUTTON_MAX];

// キャプチャコールバックとユーザーデータ
static input_capture_callback_t capture_callback;
static void *capture_user;

// マウス座標
static int32_t mouse_x;
static int32_t mouse_y;

// 入力システムの初期化
// すべてのバインディングをクリアします
void input_init(void) {
	input_unbind_all();
}

// 入力システムのクリーンアップ
// 現在は特に何もしていませんが、将来拡張できるよう用意されています
void input_cleanup(void) {
	// 将来の拡張のために予約
}

// 入力状態のクリア
// 各フレームの終わりに呼び出され、pressed/releasedフラグをリセットします
// 【C言語テクニック】マクロによる配列のクリア
void input_clear(void) {
	clear(actions_pressed);
	clear(actions_released);
}

// ボタンの状態を設定
// プラットフォーム層から呼び出され、低レベル入力をゲームアクションに変換します
// 【C言語テクニック】状態変化の検出と追跡
void input_set_button_state(button_t button, float state) {
	// 範囲チェック - 無効なボタンインデックスの場合はエラー
	error_if(button < 0 || button >= INPUT_BUTTON_MAX, "Invalid input button %d", button);

	// このボタンにバインドされたアクションを取得
	uint8_t action = bindings[button];
	if (action == INPUT_ACTION_NONE) {
		return; // このボタンはアクションにバインドされていない
	}

	// このアクションに対して期待されるボタンをチェック
	// （複数のボタンが同じアクションにバインドされている場合に使用）
	uint8_t expected = expected_button[action];
	if (!expected || expected == button) {
		// デッドゾーン処理 - 小さな入力値を0にする
		state = (state > INPUT_DEADZONE) ? state : 0;

		// 状態変化の検出
		if (state && !actions_state[action]) {
			// ボタンが押された
			actions_pressed[action] = true;
			expected_button[action] = button;
		}
		else if (!state && actions_state[action]) {
			// ボタンが離された
			actions_released[action] = true;
			expected_button[action] = INPUT_BUTTON_NONE;
		}
		// アクションの状態を更新
		actions_state[action] = state;
	}

	// キャプチャコールバックが設定されていれば呼び出す
	if (capture_callback && state > INPUT_DEADZONE_CAPTURE) {
		capture_callback(capture_user, button, 0);
	}
}

// マウス位置を設定
// プラットフォーム層から呼び出されます
void input_set_mouse_pos(int32_t x, int32_t y) {
	mouse_x = x;
	mouse_y = y;
}

// キャプチャコールバックを設定
// すべてのキー/ボタン押下をキャプチャするために使用
// 【C言語テクニック】コールバック関数による柔軟な処理の拡張
void input_capture(input_capture_callback_t cb, void *user) {
	capture_callback = cb;
	capture_user = user;
	input_clear(); // 既存の状態をクリア
}

// テキスト入力イベントの処理
// テキスト入力（文字列）をキャプチャコールバックに渡します
void input_textinput(int32_t ascii_char) {
	if (capture_callback) {
		capture_callback(capture_user, INPUT_INVALID, ascii_char);
	}
}

// ボタンをアクションにバインド
// 【C言語テクニック】引数の範囲チェックによるエラー検出
void input_bind(button_t button, uint8_t action) {
	error_if(button < 0 || button >= INPUT_BUTTON_MAX, "Invalid input button %d", button);
	error_if(action < 0 || action >= INPUT_ACTION_MAX, "Invalid input action %d", action);

	// アクションの状態をリセットしてからバインド
	actions_state[action] = 0;
	bindings[button] = action;
}

// ボタンにバインドされているアクションを取得
uint8_t input_action_for_button(button_t button) {
	error_if(button < 0 || button >= INPUT_BUTTON_MAX, "Invalid input button %d", button);
	return bindings[button];
}

// ボタンのバインドを解除
void input_unbind(button_t button) {
	error_if(button < 0 || button >= INPUT_BUTTON_MAX, "Invalid input button %d", button);

	bindings[button] = INPUT_ACTION_NONE;
}

// すべてのボタンのバインドを解除
// 【C言語テクニック】一般的な初期化パターン
void input_unbind_all(void) {	
	for (uint32_t button = 0; button < INPUT_BUTTON_MAX; button++) {
		input_unbind(button);
	}
}

// アクションの現在の状態を取得
// 0〜1の範囲の値を返します
float input_state(uint8_t action) {
	error_if(action < 0 || action >= INPUT_ACTION_MAX, "Invalid input action %d", action);
	return actions_state[action];
}

// アクションが押されたかどうかをチェック
bool input_pressed(uint8_t action) {
	error_if(action < 0 || action >= INPUT_ACTION_MAX, "Invalid input action %d", action);
	return actions_pressed[action];
}

// アクションが離されたかどうかをチェック
bool input_released(uint8_t action) {
	error_if(action < 0 || action >= INPUT_ACTION_MAX, "Invalid input action %d", action);
	return actions_released[action];
}

// 現在のマウス位置を取得
// 【C言語テクニック】マクロによるベクトル生成
vec2_t input_mouse_pos(void) {
	return vec2(mouse_x, mouse_y);
}

// 名前からボタン列挙型への変換
// 設定ファイルからの読み込みなどに便利
// 【C言語テクニック】文字列と列挙型の相互変換
button_t input_name_to_button(const char *name) {
	for (int32_t i = 0; i < INPUT_BUTTON_MAX; i++) {
		if (button_names[i] && str_equals(name, button_names[i])) {
			return i;
		}
	}
	return INPUT_INVALID;
}

// ボタン列挙型から名前への変換
// 【C言語テクニック】境界チェックとNULLチェックの組み合わせ
const char *input_button_to_name(button_t button) {
	if (
		button < 0 || button >= INPUT_BUTTON_MAX ||
		!button_names[button]
	) {
		return NULL;
	}
	return button_names[button];
}
