#ifndef HI_UTILS_H
#define HI_UTILS_H

// 様々な数学およびユーティリティ関数
// FIXME: 一部は使用されておらず、おそらく削除すべき？

#include <string.h>
#include "types.h"
#include "../libs/pl_json.h"

// Windowsとの互換性対応
#ifdef WIN32
	// Windowsの標準ヘッダーでminとmaxがマクロとして定義されているため、
	// 競合を避けるためにundefしています
	#undef min
	#undef max
#endif

// offsetofマクロ（構造体メンバーのオフセットを取得）
// 【C言語テクニック】NULL（0）ポインタを特定の型にキャストし、そのメンバーのアドレスを
// 取得することで、構造体の先頭からのバイトオフセットを計算します。
// これは標準ライブラリのoffsetofと同等の実装です。
#if !defined(offsetof)
	#define offsetof(TYPE, ELEMENT) ((size_t)&(((TYPE *)0)->ELEMENT))
#endif

// 構造体メンバーのサイズを取得するマクロ
// 【C言語テクニック】nullポインタを通じてメンバーサイズを取得する安全な方法です
#define member_size(type, member) sizeof(((type *)0)->member)

// aとbの大きい方の値を返すマクロ
// 【C言語テクニック】GCCの文ステートメント式（statement expression）を使用した
// 型安全なマクロです。__typeof__でオペランドの型を保持し、二重評価を防止しています。
#define max(a, b) ({ \
		__typeof__ (a) _a = (a); \
		__typeof__ (b) _b = (b); \
		_a > _b ? _a : _b; \
	})

// aとbの小さい方の値を返すマクロ
#define min(a, b) ({ \
		__typeof__ (a) _a = (a); \
		__typeof__ (b) _b = (b); \
		_a < _b ? _a : _b; \
	})

// aとbの値を交換するマクロ
// 【C言語テクニック】一時変数を使った基本的な交換アルゴリズムを
// マクロとして実装しています。型安全な実装です。
#define swap(a, b) ({ \
		__typeof__(a) tmp = a; a = b; b = tmp; \
	})

// 値vをmin〜maxの範囲に制限するマクロ
// 【C言語テクニック】条件演算子（三項演算子）を入れ子にして、
// 簡潔かつ効率的に範囲制限を実装しています。
#define clamp(v, min, max) ({ \
		__typeof__(v) _v = v, _min = min, _max = max; \
		_v > _max ? _max : _v < _min ? _min : _v; \
	})

// 値vを入力範囲から出力範囲にスケーリングするマクロ
// あらゆる種類の変換に便利です。たとえば、画像を画面の右側から
// 中央へ2秒かけて移動させる場合（3秒目から開始）：
// x = scale(time, 3, 5, screen_size.x, screen_size.x/2)
// 【C言語テクニック】線形変換（マッピング）の汎用的な実装です
#define scale(v, in_min, in_max, out_min, out_max) ({ \
		__typeof__(v) _in_min = in_min, _out_min = out_min; \
		_out_min + ((out_max) - _out_min) * (((v) - _in_min) / ((in_max) - _in_min)); \
	})

// 値aから値bへの線形補間（正規化された0..1のt値を使用）
// 【C言語テクニック】線形補間（Lerp）はゲーム開発やグラフィックスで頻繁に使用される
// 基本的な技術です。
#define lerp(a, b, t) ({ \
		__typeof__(a) _a = a; \
		_a + ((b) - _a) * (t); \
	})

// 角度変換マクロ
#define to_radians(A) ((A) * (M_PI/180.0))  // 度からラジアンへ変換
#define to_degrees(R) ((R) * (180.0/M_PI))  // ラジアンから度へ変換

// 静的に割り当てられた配列の要素数を取得するマクロ
// 【C言語テクニック】配列のバイトサイズを要素のバイトサイズで割ることで、
// 要素数を安全に計算します。可変引数マクロを使用しています。
#define len(...) (sizeof(__VA_ARGS__) / sizeof((__VA_ARGS__)[0]))

// 静的に割り当てられた配列または構造体をゼロクリアするマクロ
// 【C言語テクニック】メモリをゼロで初期化する効率的な方法です
#define clear(A) memset(A, 0, sizeof(A))

// 注意：これは挿入ソートです。ほぼソート済みのデータには適していますが
// （例：毎フレーム同じ配列をソートする場合）、未ソートデータでは
// O(n^2)の計算量になってしまいます。FIXME!?
// 【C言語テクニック】マクロでアルゴリズムを実装する高度な例です。
// これにより型に依存しない汎用的な実装が可能になります。
#define sort(LIST, LEN, COMPARE_FUNC) \
	for (uint32_t sort_i = 1, sort_j; sort_i < (LEN); sort_i++) { \
		sort_j = sort_i; \
		__typeof__((LIST)[0]) sort_temp = (LIST)[sort_j]; \
		while (sort_j > 0 && COMPARE_FUNC((LIST)[sort_j-1], sort_temp)) { \
			(LIST)[sort_j] = (LIST)[sort_j-1]; \
			sort_j--; \
		} \
		(LIST)[sort_j] = sort_temp; \
	}

// 公平なFisher-Yatesシャッフル
// 【C言語テクニック】統計的に偏りのない配列のランダム化アルゴリズムで、
// O(n)の計算量です。end→startの方向に処理することが重要です。
#define shuffle(LIST, LEN) \
	for (int i = (LEN) - 1; i > 0; i--) { \
		int j = rand_int(0, i); \
		swap((LIST)[i], (LIST)[j]); \
	}

// 指定した精度に値を丸めるインライン関数
// 【C言語テクニック】浮動小数点数の精度を制御する方法です
static inline float round_to_precision(float v, float p) {
	return roundf(v * p) / p;
}


// マクロを文字列に変換するためのヘルパーマクロ
// 【C言語テクニック】2段階のマクロ展開を使って、マクロの値ではなく
// マクロの名前自体を文字列化します
#define STRINGIFY(x) #x             // マクロを直接文字列化
#define TOSTRING(x) STRINGIFY(x)    // マクロを展開してから文字列化

// エラーでプログラムを終了させるマクロ
// 【C言語テクニック】ファイル名と行番号を自動的に含めることで、
// エラーの発生場所を正確に特定できます
#define die(...) \
	printf("Abort at " TOSTRING(__FILE__) " line " TOSTRING(__LINE__) ": " __VA_ARGS__); \
	printf("\n"); \
	exit(1)

// 条件が真の場合にエラーを発生させるマクロ
// 【C言語テクニック】アサーションに似た機能ですが、常に有効で、
// カスタムエラーメッセージを表示できます
#define error_if(TEST, ...) \
	if (TEST) { \
		die(__VA_ARGS__); \
	}

// 文字列haystackが文字列needleで始まるかどうかを確認
bool str_starts_with(const char *haystack, const char *needle);

// 文字列aと文字列bが等しいかどうかを確認
bool str_equals(const char *a, const char *b);

// 文字列haystack内に文字列needleが含まれるかどうかを確認
bool str_contains(const char *haystack, const char *needle);

// バンプメモリに割り当てるsprintfのラッパー
// 【C言語テクニック】可変引数関数とメモリ管理を組み合わせた便利な関数です
char *str_format(const char *format, ...);

// 乱数生成器を特定の状態にシード設定
// 【C言語テクニック】再現可能な乱数列を生成するための関数です
void rand_seed(uint64_t s);

// 乱数uint64_tを返す
uint64_t rand_uint64(void);

// min〜maxの範囲の乱数float値を返す
float rand_float(float min, float max);

// min〜max（両端を含む）の範囲の乱数int値を返す
int32_t rand_int(int32_t min, int32_t max); 


// 以下は低レベルのファイル読み込み関数で、アセットやユーザーディレクトリを
// 考慮していません。代わりにplatform_load_asset()などを使用すべきです。
// 【C言語テクニック】抽象化階層の分離の例。低レベルIOと高レベルIO関数を分けています。

// 指定パスのファイルが存在するかどうかを確認
bool file_exists(const char *path);

// パスのファイルを完全に一時メモリに読み込む。
// temp_free()で明示的に解放する必要がある。失敗時はNULLを返す。
// 【C言語テクニック】出力パラメータ（bytes_read）の使用例です
uint8_t *file_load(const char *path, uint32_t *bytes_read);

// 指定パスのファイルにlenバイトのデータを書き込む。
// 書き込まれたバイト数を返す。失敗時は0を返す。
uint32_t file_store(const char *path, void *bytes, int32_t len);

// 文字列データを一時割り当てのJSONに解析する。
// temp_free()で明示的に解放する必要がある。失敗時はNULLを返す。
// 【C言語テクニック】外部ライブラリの統合と一時メモリ管理を組み合わせています
json_t *json_parse(uint8_t *data, uint32_t len);

#endif
