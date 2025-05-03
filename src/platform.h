#ifndef HI_PLATFORM_H
#define HI_PLATFORM_H

// プラットフォーム抽象化レイヤー
// -----------------------------------------------------------------------------
// このファイルは基盤となるプラットフォーム（現在はSDLまたはSokol）を抽象化します。
//
// 【初心者向け解説】
// ゲーム開発において「プラットフォーム」とは、ウィンドウの表示、入力の処理、
// 音声の再生などの基本機能を提供するシステムのことです。これらの機能は
// オペレーティングシステム（WindowsやmacOSなど）ごとに異なる実装が必要ですが、
// SDLやSokolのようなライブラリを使うことで、それらの違いを吸収できます。
//
// このプラットフォームレイヤーの役割：
// 1. ウィンドウの作成と管理
// 2. グラフィックスレンダラーの初期化
// 3. タイミングと時間管理
// 4. 入力イベント（キーボード、マウス、ゲームパッド）の処理
// 5. ファイルの読み書き
// 6. オーディオシステムの管理
//
// SDLとSokolの違い:
// - SDL: より成熟した多機能なマルチメディアライブラリ。多くのプラットフォームをサポート。
// - Sokol: より軽量で単純なライブラリ。特にWebブラウザ（WebGL）との互換性がよい。
//
// 理論的には、他のプラットフォーム（例：特定のゲーム機向け）を追加する場合でも、
// high_impactの他の部分を変更せずに対応できるよう設計されています。
// -----------------------------------------------------------------------------

#include "types.h"
#include "../libs/pl_json.h"

// ウィンドウのタイトル（該当する場合）
// ゲームの名前としてウィンドウ上部に表示されます
#if !defined(WINDOW_TITLE)
	#define WINDOW_TITLE "High Impact Game"
#endif

// デフォルトのウィンドウサイズ（該当する場合）
// 【初心者向け】ウィンドウの幅と高さをピクセル単位で指定します
// 1280x720は一般的なHD解像度の一つです
#if !defined(WINDOW_WIDTH) || !defined(WINDOW_HEIGHT)
	#define WINDOW_WIDTH 1280
	#define WINDOW_HEIGHT 720
#endif

// 会社または開発者の名前
// ユーザーデータディレクトリに使用されることがあるため、
// 特殊文字を含めないでください
#if !defined(GAME_VENDOR)
	#define GAME_VENDOR "phoboslab"
#endif

// ゲームの名前
// ユーザーデータディレクトリに使用されることがあるため、
// 特殊文字を含めないでください
#if !defined(GAME_NAME)
	#define GAME_NAME "high_impact_game"
#endif

// VSync（垂直同期）の設定
// 【初心者向け】垂直同期とは、画面の更新タイミングをモニターのリフレッシュレート
// （通常60Hz）に合わせる機能です。これにより画面のちらつきを防ぎます。
// 1=有効、0=無効
#if !defined(PLATFORM_VSYNC)
	#define PLATFORM_VSYNC 1
#endif

// ファイルの読み書き時の最大パス長
// 【初心者向け】ファイルのパスの最大文字数を指定します
#if !defined(PLATFORM_MAX_PATH)
	#define PLATFORM_MAX_PATH 512
#endif

// ウィンドウまたはレンダリング領域の現在のサイズを実ピクセルで返す
// 【初心者向け】高DPIディスプレイでは論理サイズと実際のピクセルサイズが異なることがあります
vec2i_t platform_screen_size(void);

// プログラム開始からの現在の時間を秒単位で返す
// 【初心者向け】ゲーム内のアニメーションやタイミングに使用します
double platform_now(void);

// プログラムがフルスクリーンモードかどうかを返す
bool platform_get_fullscreen(void);

// フルスクリーンモードを設定する
void platform_set_fullscreen(bool fullscreen);

// オーディオ出力のサンプルレートを返す
// 【初心者向け】サンプルレートとは、1秒あたりの音声サンプル数です（通常44100Hzなど）
uint32_t platform_samplerate(void);

// ファイルを一時メモリに読み込む
// temp_free()で解放する必要があります
// 【初心者向け】ゲームリソース（画像やサウンドなど）を読み込むのに使います
uint8_t *platform_load_asset(const char *name, uint32_t *bytes_read);

// JSONファイルを一時メモリに読み込む
// temp_free()で解放する必要があります
// 【初心者向け】ゲーム設定やレベルデータなどの構造化されたデータを読み込むのに使います
json_t *platform_load_asset_json(const char *name);

// 現在の実行ファイルへのパスを返す
// 失敗した場合はNULLを返します
// 【初心者向け】ゲームの実行ファイルがある場所を取得します
char *platform_executable_path(void);

// パスのディレクトリ部分を返す
// 末尾にディレクトリ区切り文字（/）が追加されます
char *platform_dirname(char *path);

// ユーザーデータディレクトリからファイルを一時メモリに読み込む
// temp_free()で解放する必要があります
// これはセーブデータや設定などに使用できます
// 【初心者向け】ゲームのセーブデータなどユーザー固有のファイルを読み込みます
uint8_t *platform_load_userdata(const char *name, uint32_t *bytes_read);

// ファイルをユーザーデータディレクトリに保存する
// 【初心者向け】ゲームのセーブデータなどを保存します
uint32_t platform_store_userdata(const char *name, void *bytes, int32_t len);

// プログラムを終了する
void platform_exit(void);

// オーディオミックスコールバックを設定する
// エンジンによって行われます
// 【初心者向け】サウンドシステムが音声データを要求したときに呼び出される関数を設定します
void platform_set_audio_mix_cb(void (*cb)(float *buffer, uint32_t len));

// ソフトウェアレンダリング用の関数
// 【初心者向け】CPUで直接ピクセルを描画するモード用の関数です
#if defined(RENDER_SOFTWARE)
	rgba_t *platform_get_screenbuffer(int32_t *pitch);
#endif

// Metal（macOS/iOS向けグラフィックスAPI）用の関数
// 【初心者向け】Apple製品向けの高速グラフィックス描画に使用します
#if defined(RENDER_METAL)
	void *platform_get_metal_layer(void);
#endif

#endif
