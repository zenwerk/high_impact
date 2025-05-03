#ifndef HI_NOISE_H
#define HI_NOISE_H

// 2Dパーリンノイズジェネレーター
// 近い点に対して自然な勾配を持つ「ランダム」な数値を生成します。
// 詳細は https://en.wikipedia.org/wiki/Perlin_noise を参照
// FIXME: これはhigh_impactの一部であるべきか？
//
// 【C言語テクニック】不透明ポインタパターン（opaque pointer pattern）の使用
// 構造体の実装を隠蔽し、インターフェースのみを公開しています。
// これはカプセル化を実現する方法の一つです。

#include "types.h"

// 不透明ポインタとして定義された構造体
typedef struct noise_t noise_t;

// サイズ1 << size_bitsのノイズジェネレータを割り当て作成します
// 【C言語テクニック】ビットシフトを使用して2のべき乗のサイズを指定しています
noise_t *noise(uint8_t size_bits);

// -1から1の範囲でノイズ値を取得します
// 【C言語テクニック】座標を引数に取る純粋関数的インターフェース
float noise_gen(noise_t *n, vec2_t pos);

#endif
