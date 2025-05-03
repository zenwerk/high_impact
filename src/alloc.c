#include <stdlib.h>
#include <string.h>

#include "alloc.h"
#include "utils.h"

// メモリ管理の実装

// 固定サイズのメモリハンク（静的配列）
// 【C言語テクニック】静的配列を使用することで、動的メモリ管理のオーバーヘッドを回避しています
static uint8_t hunk[ALLOC_SIZE];  // 実際のメモリブロック

// バンプアロケータの現在位置（先頭からのオフセット）
static uint32_t bump_len = 0;

// テンポラリアロケータの使用量（末尾からのオフセット）
static uint32_t temp_len = 0;

// 現在割り当てられている一時オブジェクトのサイズを追跡する配列
// 【C言語テクニック】初期化子 {} を使って配列全体をゼロで初期化しています
static uint32_t temp_objects[ALLOC_TEMP_OBJECTS_MAX] = {};
static uint32_t temp_objects_len;  // 現在の一時オブジェクト数

// バンプアロケータの現在位置を返す
bump_mark_t bump_mark(void) {
	// 【C言語テクニック】複合リテラルを使用した構造体の初期化
	// .indexはデフォルトのC99指定初期化子構文です
	return (bump_mark_t){.index = bump_len};
}

// バンプメモリに指定サイズのバイトを割り当てる
void *bump_alloc(uint32_t size) {
	// メモリ不足チェック（バンプ領域+一時領域+新しいサイズがハンク全体を超えないか）
	error_if(bump_len + temp_len + size >= ALLOC_SIZE, "Failed to allocate %d bytes in hunk mem", size);
	
	// ハンクの現在位置からポインタを計算
	void *p = &hunk[bump_len];
	
	// バンプポインタを進める
	bump_len += size;
	
	// 割り当てたメモリをゼロクリア
	// 【C言語テクニック】memsetを使って確実に初期化し、不定値による予期せぬ動作を防止
	memset(p, 0, size);
	
	return p;
}

// バンプアロケータを指定位置にリセットする
void bump_reset(bump_mark_t mark) {
	// 不正なマークをチェック
	error_if(mark.index > ALLOC_SIZE, "Invalid mem reset");
	
	// バンプポインタを指定位置に戻す
	// これにより、それ以降に割り当てられたメモリは「論理的に」解放されます
	// （実際にはメモリの内容は変更されません）
	bump_len = mark.index;
}

// 一時メモリからバンプメモリにバイトを移動する
void *bump_from_temp(void *temp, uint32_t offset, uint32_t size) {
	// 一時メモリを解放
	temp_free(temp);
	
	// メモリ不足チェック
	error_if(bump_len + temp_len + size >= ALLOC_SIZE, "Failed to allocate %d bytes in hunk mem", size);
	
	// ハンクの現在位置からポインタを計算
	void *p = &hunk[bump_len];
	
	// バンプポインタを進める
	bump_len += size;
	
	// 一時メモリからバンプメモリにデータをコピー
	// 【C言語テクニック】memmoveを使用することで、メモリ領域が重なっていても安全にコピー
	memmove(p, (uint8_t *)temp + offset, size);
	
	return p;
}

// 一時メモリに指定サイズのバイトを割り当てる
void *temp_alloc(uint32_t size) {
	// 8バイトアラインメントに調整（パフォーマンス向上のため）
	// 【C言語テクニック】ビット操作による効率的なアラインメント
	// ((size + 7) >> 3) << 3 は size を 8の倍数に切り上げる
	size = ((size + 7) >> 3) << 3; // 8バイト境界に合わせる

	// メモリ不足チェック
	error_if(bump_len + temp_len + size >= ALLOC_SIZE, "Failed to allocate %d bytes in temp mem", size);
	
	// 一時オブジェクト数の上限チェック
	error_if(temp_objects_len >= ALLOC_TEMP_OBJECTS_MAX, "ALLOC_TEMP_OBJECTS_MAX reached");

	// 一時メモリの使用量を更新
	temp_len += size;
	
	// ハンクの末尾からポインタを計算（一時メモリはハンクの末尾から逆方向に成長）
	void *p = &hunk[ALLOC_SIZE - temp_len];
	
	// 割り当てたオブジェクトを追跡リストに追加
	temp_objects[temp_objects_len++] = temp_len;
	
	return p;
}

// 一時割り当てを解放する
void temp_free(void *p) {
	// ポインタをオフセットに変換（ハンク末尾からの距離）
	// 【C言語テクニック】ポインタ演算を使用して、ハンク内の位置を確認
	uint32_t offset = (uint8_t *)&hunk[ALLOC_SIZE] - (uint8_t *)p;
	
	// 不正なポインタをチェック
	error_if(offset > ALLOC_SIZE, "Object 0x%p not in temp hunk", p);

	// オブジェクトを追跡リストから探して削除
	bool found = false;
	uint32_t remaining_max = 0;
	
	for (uint32_t i = 0; i < temp_objects_len; i++) {
		if (temp_objects[i] == offset) {
			// オブジェクトが見つかったら削除（最後の要素と入れ替えて配列を縮小）
			// 【C言語テクニック】カウンタ変数を調整して、配列要素の入れ替え後も正しく処理を継続
			temp_objects[i--] = temp_objects[--temp_objects_len];
			found = true;
		}
		else if (temp_objects[i] > remaining_max) {
			// 残りのオブジェクトの中で最大のオフセットを記録
			remaining_max = temp_objects[i];
		}
	}
	
	// オブジェクトが見つからなかった場合はエラー
	error_if(!found, "Object 0x%p not in temp hunk", p);
	
	// 一時メモリの使用量を最大のオフセットに更新
	// （これにより、不連続な解放でもメモリを効率的に管理）
	temp_len = remaining_max;
}

// 一時アロケータが空かどうかをチェック
void temp_alloc_check(void) {
	// 一時メモリが空でない場合はエラー
	// 【C言語テクニック】デバッグ支援機能として、メモリリークを検出
	error_if(temp_len != 0, "Temp memory not free: %d object(s)", temp_objects_len);
}
