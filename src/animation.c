#include <string.h>

#include "animation.h"
#include "alloc.h"
#include "render.h"
#include "engine.h"

// アニメーション定義を作成する関数
// 指定されたシート、フレームサイズ、フレーム時間、シーケンス、シーケンス長から
// 新しいアニメーション定義を作成します
anim_def_t *anim_def_with_len(image_t *sheet, vec2i_t frame_size, float frame_time, uint16_t *sequence, uint16_t sequence_len) {
	// ゲームプレイ中にアニメーション定義を作成できないようにする安全チェック
	// （アセットはゲーム開始前にロードすべき）
	error_if(engine_is_running(), "Cannot create anim_def during gameplay");

	// デフォルトではループする
	bool loop = true;
	
	// 空のシーケンスをチェック
	error_if(sequence_len == 0, "Animation has empty sequence");
	
	// シーケンス内のANIM_STOP値を探す
	// 【C言語テクニック】特殊値を使った配列終端のマーキング
	for (int i = 0; i < sequence_len; i++) {
		if (sequence[i] == ANIM_STOP) {
			// ANIM_STOPは最後のフレームにのみ配置可能
			error_if(i == 0 || i != sequence_len - 1, "Animation can only stop on last frame");
			// 実際のシーケンス長を調整
			sequence_len = i-1;
			// ループフラグをオフに
			loop = false;
			break;
		}
	}

	// アニメーション定義用のメモリを割り当て
	// 【C言語テクニック】可変長配列を含む構造体のメモリ割り当て
	// 構造体本体と可変長配列部分のサイズを合わせて割り当てる
	anim_def_t *def = bump_alloc(sizeof(anim_def_t) + sizeof(uint16_t) * sequence_len);
	
	// 定義のフィールドを初期化
	def->sheet = sheet;
	def->frame_time = frame_time;
	// 最適化のために全体時間の逆数を事前計算
	def->inv_total_time = 1.0 / (sequence_len * frame_time);
	def->frame_size = frame_size;
	def->loop = loop;
	def->sequence_len = sequence_len;

	// シーケンスデータをコピー
	// 【C言語テクニック】memcpyを使用した効率的な配列コピー
	memcpy(def->sequence, sequence, sequence_len * sizeof(uint16_t));
	
	return def;
}

// アニメーションをシーケンスの最初のフレームに巻き戻す
void anim_rewind(anim_t *anim) {
	// 開始時間を現在の時間に設定し、アニメーションをリセット
	anim->start_time = engine.time;
}

// アニメーションを指定したフレームに移動する
void anim_goto(anim_t *anim, int frame) {
	// 開始時間を調整して、現在のフレームが指定したフレームになるようにする
	// 【C言語テクニック】時間ベースのアニメーション制御
	// 現在時間から必要な分だけ「過去に戻す」ことで、特定のフレームに移動しています
	anim->start_time = engine.time - frame * anim->def->frame_time;
}

// アニメーションが何回ループしたかを返す
uint32_t anim_looped(anim_t *anim) {
	// アニメーション開始からの経過時間を計算
	double diff = engine.time - anim->start_time;
	
	// 経過時間をアニメーション全体の時間で割って、ループ回数を取得
	// 【C言語テクニック】あらかじめ計算した逆数（inv_total_time）を使って
	// 除算を乗算に置き換えることで計算を高速化しています
	return (uint32_t)(diff * anim->def->inv_total_time);
}

// アニメーションをランダムなフレームに移動する
void anim_goto_rand(anim_t *anim) {
	// シーケンス内のランダムなフレームにジャンプ
	anim_goto(anim, rand_int(0, anim->def->sequence_len-1));
}

// 指定した位置にアニメーションを描画する
void anim_draw(anim_t *anim, vec2_t pos) {
	// アニメーション定義を取得
	anim_def_t *def = anim->def;
	
	// 画面サイズを取得
	vec2i_t rs = render_size();
	
	// カリング（画面外や透明の場合は描画をスキップ）
	// 【C言語テクニック】早期リターンパターンで、不要な処理を回避
	if (
		pos.x > rs.x || pos.y > rs.y ||                            // 画面右下外
		pos.x + def->frame_size.x < 0 || pos.y + def->frame_size.y < 0 || // 画面左上外
		anim->color.a <= 0                                         // 完全に透明
	) {
		return;
	}

	// アニメーション開始からの経過時間を計算（負の値は0にクランプ）
	double diff = max(0, engine.time - anim->start_time);
	
	// 経過時間をアニメーション全体の時間で割って、ループ回数を取得
	double looped = diff * def->inv_total_time;

	// 現在のフレームインデックスを計算
	// 【C言語テクニック】三項演算子を使った条件分岐
	// ループしないアニメーションで1周以上経過した場合は最後のフレームを表示
	// それ以外の場合は、経過時間の小数部分に基づいてフレームを計算
	int frame = !def->loop && looped >= 1
		? def->sequence_len - 1
		: (looped - (int)looped) * def->sequence_len;
		
	// タイル番号を計算（シーケンス内の値＋タイルオフセット）
	int tile = def->sequence[frame] + anim->tile_offset;

	// 回転がない場合は通常描画
	if (anim->rotation == 0) {
		image_draw_tile_ex(
			def->sheet, tile, def->frame_size, pos, 
			anim->flip_x, anim->flip_y, anim->color
		);
	}
	// 回転がある場合は変換行列を使用
	else {
		// 変換行列をスタックにプッシュ（状態を保存）
		render_push();
		// 回転の中心点に移動
		render_translate(vec2_add(pos, def->pivot));
		// 回転を適用
		render_rotate(anim->rotation);
		// タイルを描画（ピボットを基準にオフセット）
		image_draw_tile_ex(
			def->sheet, tile, def->frame_size, vec2_mulf(def->pivot, -1), 
			anim->flip_x, anim->flip_y, anim->color
		);
		// 変換行列をポップ（状態を復元）
		render_pop();
	}
}
