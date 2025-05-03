#ifndef HI_TYPES_H
#define HI_TYPES_H

// 様々な型と付随する関数
// TODO: ドキュメント化

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

// M_PIが定義されていない場合の対策
#if !defined(M_PI)
    #define M_PI 3.14159265358979323846
#endif

// ベクトル比較の際の微小誤差の許容値
#define VEC2_EQ_EPSILON 0.0001

// 2次元の浮動小数点ベクトル型
typedef struct {
	float x, y;  // x, y座標
} vec2_t;

// 2次元の整数ベクトル型
typedef struct {
	int x, y;    // x, y座標（整数）
} vec2i_t;

// 3x3変換行列型（2D変換用）
typedef struct {
	float a, b, c, d, tx, ty;  // 2D変換行列の要素
} mat3_t;

// RGBA色表現のための共用体型
typedef union rgba_t {
	struct {
		uint8_t r, g, b, a;    // 赤、緑、青、アルファの各成分
	};
	uint8_t components[4];     // 配列としてのアクセス用
	uint32_t v;                // 32ビット整数としてのアクセス用
} rgba_t;

// ベクトル作成用マクロ
#define vec2(X, Y) ((vec2_t){.x = X, .y = Y})         // 浮動小数点ベクトル作成
#define vec2i(X, Y) ((vec2i_t){.x = X, .y = Y})       // 整数ベクトル作成

// 行列作成用マクロ
#define mat3(a, b, c, d, tx, ty) (mat3_t){a, b, c, d, tx, ty}  // 変換行列作成
#define mat3_identity() mat3(1,0,0,1,0,0)                      // 単位行列作成

// 色作成用マクロ
#define rgba(R, G, B, A) ((rgba_t){.r = R, .g = G, .b = B, .a = A})  // RGBA色作成
#define rgba_white() ((rgba_t){.v = 0xffffffff})                     // 白色作成

// vec2_t用の各種ユーティリティ関数（すべてinline）
static inline vec2_t vec2_from_vec2i(vec2i_t a)     { return vec2(a.x, a.y); }             // 整数ベクトルから浮動小数点ベクトルへ変換
static inline vec2_t vec2_from_angle(float a)       { return vec2(cosf(a), sinf(a));}      // 角度からベクトルを作成
static inline float  vec2_to_angle(vec2_t a)        { return atan2(a.y, a.x); }            // ベクトルから角度を取得
static inline vec2_t vec2_add(vec2_t a, vec2_t b)   { return vec2(a.x + b.x, a.y + b.y); } // ベクトル加算
static inline vec2_t vec2_sub(vec2_t a, vec2_t b)   { return vec2(a.x - b.x, a.y - b.y); } // ベクトル減算
static inline vec2_t vec2_mulf(vec2_t a, float f)   { return vec2(a.x * f, a.y * f); }     // ベクトルをスカラー倍
static inline vec2_t vec2_divf(vec2_t a, float f)   { return vec2(a.x / f, a.y / f); }     // ベクトルをスカラーで除算
static inline vec2_t vec2_mul(vec2_t a, vec2_t b)   { return vec2(a.x * b.x, a.y * b.y); } // ベクトル要素同士の積（成分ごと）
static inline vec2_t vec2_div(vec2_t a, vec2_t b)   { return vec2(a.x / b.x, a.y / b.y); } // ベクトル要素同士の商（成分ごと）
static inline vec2_t vec2_abs(vec2_t a)             { return vec2(fabsf(a.x), fabsf(a.y)); } // ベクトルの絶対値
static inline float  vec2_len(vec2_t a)             { return sqrtf(a.x * a.x + a.y * a.y); } // ベクトルの長さ
static inline float  vec2_dist(vec2_t a, vec2_t b)  { return vec2_len(vec2_sub(a, b)); }   // 2点間の距離
static inline float  vec2_dot(vec2_t a, vec2_t b)   { return a.x * b.x + a.y * b.y; }      // ベクトルの内積
static inline float  vec2_cross(vec2_t a, vec2_t b) { return a.x * b.y - a.y * b.x; }      // ベクトルの外積（z成分）
static inline bool   vec2_eq(vec2_t a, vec2_t b)    { return fabsf(a.x - b.x) + fabsf(a.y - b.y) < VEC2_EQ_EPSILON; } // ベクトルが近似的に等しいか

static inline vec2i_t vec2i_from_vec2(vec2_t a)       { return vec2i(a.x, a.y); }
static inline vec2i_t vec2i_add(vec2i_t a, vec2i_t b) { return vec2i(a.x + b.x, a.y + b.y); }
static inline vec2i_t vec2i_sub(vec2i_t a, vec2i_t b) { return vec2i(a.x - b.x, a.y - b.y); }
static inline vec2i_t vec2i_muli(vec2i_t a, int f)    { return vec2i(a.x * f, a.y * f); }
static inline vec2i_t vec2i_divi(vec2i_t a, int f)    { return vec2i(a.x / f, a.y / f); }
static inline vec2i_t vec2i_mul(vec2i_t a, vec2i_t b) { return vec2i(a.x * b.x, a.y * b.y); }
static inline vec2i_t vec2i_div(vec2i_t a, vec2i_t b) { return vec2i(a.x / b.x, a.y / b.y); }
static inline vec2i_t vec2i_abs(vec2i_t a)            { return vec2i(abs(a.x), abs(a.y)); }
static inline bool    vec2i_eq(vec2i_t a, vec2i_t b)  { return a.x == b.x && a.y == b.y; }

static inline float vec2_angle(vec2_t a, vec2_t b) {
	vec2_t d = vec2_sub(b, a);
	return atan2(d.y, d.x); 
}

static inline vec2_t vec2_transform(vec2_t v, mat3_t *m) {
	return vec2(
		m->a * v.x + m->b * v.y + m->tx,
		m->c * v.x + m->d * v.y + m->ty
	);
}

static inline float wrap_angle(float a) {
	a = fmodf(a + M_PI, M_PI * 2);
	if (a < 0) {
		a += M_PI * 2;
	}
	return a - M_PI;
}

static inline mat3_t *mat3_translate(mat3_t *m, vec2_t t) {
	m->tx += m->a * t.x + m->c * t.y;
	m->ty += m->b * t.x + m->d * t.y;
	return m;
}

static inline mat3_t *mat3_scale(mat3_t *m, vec2_t r) {
	m->a *= r.x;
	m->b *= r.x;
	m->c *= r.y;
	m->d *= r.y;
	return m;
}

static inline mat3_t *mat3_rotate(mat3_t *m, float r) {
	float sin = sinf(r);
	float cos = cosf(r);
	float a = m->a, b = m->b;
	float c = m->c, d = m->d;

	m->a = a * cos + c * sin;
	m->b = b * cos + d * sin;
	m->c = c * cos - a * sin;
	m->d = d * cos - b * sin;
	return m;
}

static inline rgba_t rgba_blend(rgba_t in, rgba_t out) {
	int in_a = 255 - out.a;
	return rgba(
		(in.r * in_a + out.r * out.a) >> 8,
		(in.g * in_a + out.g * out.a) >> 8,
		(in.b * in_a + out.b * out.a) >> 8,
		1
	);
}

static inline rgba_t rgba_mix(rgba_t a, rgba_t b) {
	return rgba(
		(a.r * b.r) >> 8,
		(a.g * b.g) >> 8,
		(a.b * b.b) >> 8,
		(a.a * b.a) >> 8
	);
}



#endif
