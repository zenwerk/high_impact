#ifndef HI_RENDER_H
#define HI_RENDER_H

// レンダラーシステム（描画システム）
// =============================================================================
// 【レンダリングとは】
// レンダリングとは、画面上に物を描画するプロセスです。ゲームやグラフィックアプリ
// では、画像、テキスト、図形などの視覚要素をディスプレイに表示する必要があります。
// このプロセスを担当するのがレンダラーです。
//
// 【複数のレンダラーバックエンド】
// このエンジンは複数の「レンダラーバックエンド」を使用できるよう設計されています：
// - OpenGL: 多くのプラットフォームで動作する3Dグラフィックスライブラリ
// - Metal: AppleのmacOSとiOS向け高速グラフィックスAPI
// - ソフトウェア: CPUのみで計算するシンプルなレンダラー（グラフィックスカード不要）
//
// これにより、様々な環境（Windows、Mac、Linux、Web）でゲームが動作できます。
// 異なるレンダラーは同じインターフェースを実装しているため、ゲームコード側は
// どのレンダラーが使われているかを気にする必要がありません。
//
// A renderer is responsible for drawing on the screen. Images, Fonts and
// Animations ulitmately use the render_* functions to be drawn.
// Different renderer backends can be implemented by supporting just a handful
// of functions.

#include "types.h"


// 論理解像度（ロジカルサイズ）設定
// -----------------------------------------------------------------------------
// 【初心者向け解説】
// 「論理解像度」とは、実際のウィンドウサイズとは別に、ゲームが内部的に使用する解像度です。
// 例えば、ウィンドウサイズが640x480ピクセルでも、ゲーム内部の論理解像度を320x240に
// 設定することができます。
//
// これにより、様々な画面サイズのデバイスで一貫した見た目を維持できます。
// 例えば、小さなスマホの画面でも大きなモニターでも、同じゲーム要素が同じ相対的な
// 大きさで表示されるようになります。
//
// 【メリット】
// - 異なる解像度の画面で一貫した見た目を実現
// - ピクセルアートゲームでの拡大縮小の管理が容易
// - UIレイアウトが単純化される
//
// The desired "logical size" or viewport size of the screen. This may be 
// different from the real pixel size. E.g. you can have a window with size of 
// 640x480 and a render size of 320x240. Note that, depending on the RESIZE_MODE 
// this logical size may also change when you resize the window.
#if !defined(RENDER_WIDTH) || !defined(RENDER_HEIGHT)
	#define RENDER_WIDTH 1280
	#define RENDER_HEIGHT 720
#endif

// スケールモード設定
// -----------------------------------------------------------------------------
// 【初心者向け解説】
// スケールモードは、論理解像度をウィンドウサイズに合わせてどのように拡大するかを決定します。
//
// - RENDER_SCALE_NONE（スケールなし）: 
//   拡大しない。ウィンドウが大きくなっても、描画領域は元のサイズのまま
//
// - RENDER_SCALE_DISCRETE（離散スケール）: 
//   整数倍（1倍、2倍、3倍...）のみでスケール。ピクセルアートゲームで
//   くっきりとした見た目を保つのに最適
//
// - RENDER_SCALE_EXACT（正確スケール）: 
//   ウィンドウサイズに正確に合わせる。滑らかだが、ピクセルアートでは
//   ぼやけて見える可能性あり
//
// The scale mode determines if and how the logical size will be scaled up when
// the window is larger than the render size. Note that the desired aspect ratio
// will be maintained (depending on RESIZE_MODE).
// RENDER_SCALE_NONE     - no scaling
// RENDER_SCALE_DISCRETE - scale in integer steps for perfect pixel scaling
// RENDER_SCALE_EXACT    - scale exactly to the window size
#if !defined(RENDER_SCALE_MODE)
	#define RENDER_SCALE_MODE RENDER_SCALE_DISCRETE
#endif

// リサイズモード設定
// -----------------------------------------------------------------------------
// 【初心者向け解説】
// リサイズモードは、ウィンドウサイズが変更されたとき、論理解像度をどう調整するかを
// 決定します。
//
// - RENDER_RESIZE_NONE（リサイズなし）: 
//   論理解像度は常に固定。アスペクト比が維持されるが、ウィンドウ全体を使い切れない
//
// - RENDER_RESIZE_WIDTH（幅のみリサイズ）: 
//   高さを固定し、幅だけを調整。横長のゲームに適している
//
// - RENDER_RESIZE_HEIGHT（高さのみリサイズ）: 
//   幅を固定し、高さだけを調整。縦長のゲームに適している
//
// - RENDER_RESIZE_ANY（両方リサイズ）: 
//   幅と高さの両方を調整。ウィンドウ全体を使い切るが、アスペクト比によっては
//   見た目が伸びる可能性がある
//
// The resize mode determines how the logical size changes to adapt to the
// available window size.
// RENDER_RESIZE_NONE    - don't resize
// RENDER_RESIZE_WIDTH   - resize only width; keep height fixed at RENDER_HEIGHT
// RENDER_RESIZE_HEIGHT  - resize only height; keep width fixed at RENDER_WIDTH
// RENDER_RESIZE_ANY     - resize width and height to fill the window
#if !defined(RENDER_RESIZE_MODE)
	#define RENDER_RESIZE_MODE RENDER_RESIZE_ANY
#endif

// トランスフォームスタックの最大サイズ
// -----------------------------------------------------------------------------
// 【初心者向け解説】
// トランスフォームは、描画する位置や回転、拡大縮小などの情報です。
// ゲームでは、例えば「キャラクターを中心に回転する武器」のような複雑な描画が
// 必要になります。スタックとは、これらの変換を階層的に管理するためのデータ構造です。
// render_push()とrender_pop()でトランスフォームの階層を操作できます。
#if !defined(RENDER_TRANSFORM_STACK_SIZE)
	#define RENDER_TRANSFORM_STACK_SIZE 16
#endif

// 同時に読み込めるテクスチャの最大数
// -----------------------------------------------------------------------------
// 【初心者向け解説】
// テクスチャとは、画像データをGPUメモリに保存したものです。
// この値は、同時に扱えるテクスチャの枚数を制限します。
// 多くのテクスチャを同時に使用するゲームでは、この値を増やす必要があるかもしれません。
#if !defined(RENDER_TEXTURES_MAX)
	#define RENDER_TEXTURES_MAX 1024
#endif

// スケールモードの列挙型
// -----------------------------------------------------------------------------
// 【初心者向け解説】
// 前述のスケールモードを表す列挙型（enum）です。
// ゲームコードでは、これらの定数を使ってスケールモードを指定します。
typedef enum {
	RENDER_SCALE_NONE,     // スケールなし
	RENDER_SCALE_DISCRETE, // 整数倍スケール
	RENDER_SCALE_EXACT     // 正確なスケール
} render_scale_mode_t;

// リサイズモードの列挙型
// -----------------------------------------------------------------------------
// 【初心者向け解説】
// 前述のリサイズモードを表す列挙型です。
// 値に数字が割り当てられているのは、ビットフラグとして使用するためです。
typedef enum  {
	RENDER_RESIZE_NONE    = 0, // リサイズなし
	RENDER_RESIZE_WIDTH   = 1, // 幅のみリサイズ
	RENDER_RESIZE_HEIGHT  = 2, // 高さのみリサイズ
	RENDER_RESIZE_ANY     = 3, // 両方リサイズ
} render_resize_mode_t;

// ブレンドモードの列挙型
// -----------------------------------------------------------------------------
// 【初心者向け解説】
// ブレンドモードは、テクスチャを描画するときに、既存のピクセルとどのように
// 合成するかを指定します。
//
// - RENDER_BLEND_NORMAL（通常ブレンド）:
//   標準的な透明度ブレンド。アルファ値に基づいて新しいピクセルと既存のピクセルを混ぜる
//
// - RENDER_BLEND_LIGHTER（加算ブレンド）: 
//   新しいピクセルの色を既存のピクセルに加算する。光や炎、エフェクトなどの表現に適している
typedef enum {
	RENDER_BLEND_NORMAL,  // 通常の透明度ブレンド
	RENDER_BLEND_LIGHTER  // 加算ブレンド（光エフェクトなどに使用）
} render_blend_mode_t;

// ポストエフェクトの列挙型
// -----------------------------------------------------------------------------
// 【初心者向け解説】
// ポストエフェクトとは、画面全体に適用される視覚効果です。
// 「ポスト」は「後で」という意味で、通常の描画の後に適用されます。
//
// - RENDER_POST_NONE（エフェクトなし）: 
//   効果を適用しない通常の描画
//
// - RENDER_POST_CRT（ブラウン管効果）: 
//   古いテレビやアーケードゲームのブラウン管のような見た目を再現する効果
typedef enum {
	RENDER_POST_NONE,  // ポストエフェクトなし
	RENDER_POST_CRT,   // CRT（ブラウン管）風エフェクト
	RENDER_POST_MAX,   // エフェクトの最大数（内部使用）
} render_post_effect_t;

// 頂点（バーテックス）構造体
// -----------------------------------------------------------------------------
// 【初心者向け解説】
// 3Dグラフィックスでは、すべての形状は頂点（点）の集まりで表現されます。
// この構造体は一つの頂点を表し、以下の情報を含みます：
//
// - pos: 位置座標（x, y）
// - uv: テクスチャ座標（テクスチャのどの部分を使うか）
// - color: 頂点の色（赤、緑、青、透明度）
//
// 2Dゲームでも3Dグラフィックス技術を使うため、これらの概念が必要です。
typedef struct {
	vec2_t pos;   // 位置座標
	vec2_t uv;    // テクスチャ座標
	rgba_t color; // 色と透明度
} vertex_t;

// 四角形（クワッド）の頂点データ構造体
// -----------------------------------------------------------------------------
// 【初心者向け解説】
// 画面に表示される基本図形は四角形です。四角形は4つの頂点で表現されます。
// この構造体は、四角形を描画するための4つの頂点データを保持します。
typedef struct {
	vertex_t vertices[4]; // 4つの頂点
} quadverts_t;

// テクスチャ関連の型定義
// -----------------------------------------------------------------------------
// 【初心者向け解説】
// これらは、テクスチャ（画像データ）を識別するための型です。
// 実際には単なる番号（index）ですが、型安全性のために構造体として定義されています。
//
// - texture_t: テクスチャを参照するための型
// - texture_mark_t: テクスチャの状態を記録するための型（リセット機能で使用）
//
// RENDER_NO_TEXTUREは、テクスチャなしで描画するときに使用する特殊な値です。
typedef struct { uint32_t index; } texture_mark_t; // テクスチャの状態を記録
typedef struct { uint32_t index; } texture_t;      // テクスチャ参照
extern texture_t RENDER_NO_TEXTURE;                // テクスチャなし定数


// 初期化・終了関数（プラットフォーム層から呼び出される）
// -----------------------------------------------------------------------------
// 【初心者向け解説】
// これらの関数は、ゲームの開始時と終了時にプラットフォーム層から呼び出されます。
// レンダリングシステムの初期化や後片付けを行います。
//
// レンダラーの初期化時には画面サイズが渡され、それに基づいて内部状態を設定します。
// 終了時には確保したリソースを解放します。
void render_init(vec2i_t screen_size);  // レンダラーを初期化
void render_cleanup(void);              // レンダラーのリソースを解放

// パフォーマンス計測関数
// -----------------------------------------------------------------------------
// 【初心者向け解説】
// 描画命令（ドローコール）の回数を取得します。ドローコールが多いほど、
// グラフィックスカードへの命令が増え、パフォーマンスが低下する可能性があります。
// デバッグや最適化に役立ちます。
uint32_t render_draw_calls(void);

// 画面サイズ管理関数
// -----------------------------------------------------------------------------
// 【初心者向け解説】
// ウィンドウサイズが変更されたとき、論理解像度を再計算して調整します。
// また、現在の論理解像度を取得することもできます。
//
// resize関数は、ウィンドウがリサイズされたときにプラットフォーム層から呼び出されます。
// size関数は、ゲームコードが論理解像度を知る必要があるときに使用します。
void render_resize(vec2i_t avaiable_size);  // 論理サイズを再計算（ウィンドウサイズ変更時）
vec2i_t render_size(void);                  // 現在の論理サイズを取得

// トランスフォーム操作関数群
// -----------------------------------------------------------------------------
// 【初心者向け解説】
// トランスフォーム（変換）操作は、描画位置や回転、拡大縮小を制御します。
// これらの関数を使うことで、例えば「キャラクターの位置を中心に、武器を回転させて描画」
// といった複雑な描画を簡単に実現できます。
//
// 仕組み：
// 1. render_push()でトランスフォームスタックに現在の状態を保存
// 2. translate/scale/rotateで位置/拡大/回転を変更
// 3. 描画を実行
// 4. render_pop()で元の状態に戻る
//
// 例：キャラクターを中心に回転する剣を描画
// render_push();          // 現在の状態を保存
// render_translate(キャラの位置); // キャラの位置に移動
// render_rotate(回転角度);       // 回転
// render_draw(...剣のテクスチャ...); // 剣を描画
// render_pop();           // 元の状態に戻る
void render_push(void);                   // 現在のトランスフォームをスタックに保存
void render_pop(void);                    // トランスフォームスタックから復元
void render_translate(vec2_t translate);  // 描画位置を移動
void render_scale(vec2_t scale);          // 拡大縮小を設定
void render_rotate(float rotation);       // 回転を設定

// ピクセル精度の制御
// -----------------------------------------------------------------------------
// 【初心者向け解説】
// ピクセルパーフェクトな描画（ぼやけない、くっきりした描画）を行うための関数です。
// 特にピクセルアートゲームで重要になります。論理座標を実際のスクリーンピクセルに
// 合わせて調整します。
vec2_t render_snap_px(vec2_t pos);  // 座標を実際のピクセルにスナップ（吸着）

// 描画関数
// -----------------------------------------------------------------------------
// 【初心者向け解説】
// この関数が、実際にテクスチャを画面に描画するための中心的な関数です。
// 様々なパラメータを組み合わせて、位置、大きさ、テクスチャの一部分の使用、
// 色や透明度の調整などを制御できます。
//
// - pos: 描画位置（論理座標）
// - size: 描画サイズ（論理単位）
// - texture_handle: 描画するテクスチャ
// - uv_offset/uv_size: テクスチャの中で使用する領域（テクスチャアトラスのサブ画像など）
// - color: 色調整と透明度
void render_draw(
    vec2_t pos,                // 描画位置
    vec2_t size,               // 描画サイズ
    texture_t texture_handle,  // テクスチャ
    vec2_t uv_offset,          // テクスチャ内の開始位置
    vec2_t uv_size,            // テクスチャ内で使用するサイズ
    rgba_t color               // 色と透明度
);



// レンダラーバックエンドが実装すべき関数群
// =============================================================================
// 【初心者向け解説】
// 以下の関数は、各レンダラー（OpenGL, Metal, ソフトウェア）が独自に実装する必要がある
// インターフェースです。これにより、エンジンは異なるグラフィックスAPIを使用しても
// 同じ方法で描画命令を出すことができます。
//
// render_gl.c, render_metal.m, render_software.c などのファイルで
// これらの関数が実装されています。

// バックエンド初期化・クリーンアップ関数
// -----------------------------------------------------------------------------
// レンダラー固有のリソース確保と解放
void render_backend_init(void);
void render_backend_cleanup(void);

// 描画設定関数
// -----------------------------------------------------------------------------
// 画面サイズ、ブレンドモード、ポストエフェクトなどの設定
void render_set_screen(vec2i_t size);
void render_set_blend_mode(render_blend_mode_t mode);
void render_set_post_effect(render_post_effect_t post);

// フレーム管理関数
// -----------------------------------------------------------------------------
// フレーム開始時と終了時の処理
void render_frame_prepare(void);
void render_frame_end(void);

// 描画関数
// -----------------------------------------------------------------------------
// 基本的な四角形描画
void render_draw_quad(quadverts_t *quad, texture_t texture_handle);

// テクスチャ管理関数
// -----------------------------------------------------------------------------
// テクスチャの作成、更新、管理
texture_mark_t textures_mark(void);  // 現在のテクスチャ状態をマーク
void textures_reset(texture_mark_t mark);  // マークした状態に戻す
texture_t texture_create(vec2i_t size, rgba_t *pixels);  // 新しいテクスチャを作成
void texture_replace_pixels(texture_t texture_handle, vec2i_t size, rgba_t *pixels);  // テクスチャのピクセルを更新

#endif
