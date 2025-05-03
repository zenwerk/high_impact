#include "camera.h"
#include "entity.h"
#include "engine.h"
#include "render.h"

// カメラのビューポートターゲット（表示領域の目標位置）を計算する内部関数
// 【C言語テクニック】static関数による内部実装の隠蔽。モジュール内でのみ使用可能な
// ヘルパー関数として宣言しています
static vec2_t camera_viewport_target(camera_t *cam) {
	// 画面サイズを取得
	vec2_t screen_size = vec2_from_vec2i(render_size());
	
	// 画面中心を計算
	vec2_t screen_center = vec2_mulf(screen_size, 0.5);
	
	// ビューポートの目標位置をカメラ位置、画面中心、オフセットから計算
	// 【C言語テクニック】数学的変換の連鎖を明確に表現するために、
	// 複数の関数呼び出しを入れ子にしています
	vec2_t viewport_target = vec2_add(vec2_sub(cam->pos, screen_center), cam->offset);

	// コリジョンマップが設定されている場合は、ビューポートがマップ境界内に
	// 収まるようにクランプ（制限）する
	if (engine.collision_map) {
		// マップの境界サイズを計算
		vec2_t bounds = vec2_from_vec2i(vec2i_muli(engine.collision_map->size, engine.collision_map->tile_size));
		
		// X座標をマップ境界内にクランプ
		viewport_target.x = clamp(viewport_target.x, 0, bounds.x - screen_size.x);
		
		// Y座標をマップ境界内にクランプ
		viewport_target.y = clamp(viewport_target.y, 0, bounds.y - screen_size.y);
	}
	
	return viewport_target;
}

// カメラを更新し、追跡対象に向かって移動させる関数
void camera_update(camera_t *cam) {
	// 追跡対象のエンティティを取得
	entity_t *follow = entity_by_ref(cam->follow);
	
	// 追跡対象が存在する場合
	if (follow) {
		// デッドゾーンとエンティティサイズの小さい方を使用
		// （大きすぎるエンティティのためのサイズ制限）
		vec2_t size = vec2(
			min(follow->size.x, cam->deadzone.x),
			min(follow->size.y, cam->deadzone.y)
		);

		// X軸方向のデッドゾーン処理
		// エンティティがデッドゾーンの左端を超えた場合
		if (follow->pos.x < cam->deadzone_pos.x) {
			// デッドゾーンの位置を更新
			cam->deadzone_pos.x = follow->pos.x;
			// 左方向への先読み設定
			cam->look_ahead_target.x = -cam->look_ahead.x;
		}
		// エンティティがデッドゾーンの右端を超えた場合
		else if (follow->pos.x + size.x > cam->deadzone_pos.x + cam->deadzone.x) {
			// デッドゾーンの位置を更新
			cam->deadzone_pos.x = follow->pos.x + size.x - cam->deadzone.x;
			// 右方向への先読み設定
			cam->look_ahead_target.x = cam->look_ahead.x;
		}

		// Y軸方向のデッドゾーン処理
		// エンティティがデッドゾーンの上端を超えた場合
		if (follow->pos.y < cam->deadzone_pos.y) {
			cam->deadzone_pos.y = follow->pos.y;
			// 上方向への先読み設定
			cam->look_ahead_target.y = -cam->look_ahead.y;
		}
		// エンティティがデッドゾーンの下端を超えた場合
		else if (follow->pos.y + size.y > cam->deadzone_pos.y + cam->deadzone.y) {
			cam->deadzone_pos.y = follow->pos.y + size.y - cam->deadzone.y;
			// 下方向への先読み設定
			cam->look_ahead_target.y = cam->look_ahead.y;
		}

		// プラットフォームへのスナップが有効で、エンティティが地面に接地している場合
		// 【C言語テクニック】フラグによる条件付き動作
		if (cam->snap_to_platform && follow->on_ground) {
			// デッドゾーンの下端をエンティティの足元に合わせる
			cam->deadzone_pos.y = follow->pos.y + follow->size.y - cam->deadzone.y;
		}
		
		// デッドゾーンの中央を計算
		vec2_t deadzone_target = vec2_add(cam->deadzone_pos, vec2_mulf(cam->deadzone, 0.5));
		// カメラの最終位置を、デッドゾーン中央と先読み位置の合計に設定
		cam->pos = vec2_add(deadzone_target, cam->look_ahead_target);
	}	

	// 現在のビューポートと目標位置の差分を計算
	vec2_t diff = vec2_sub(camera_viewport_target(cam), engine.viewport);
	// 差分に速度係数を掛けてカメラの速度を計算
	cam->vel = vec2_mulf(diff, cam->speed);

	// カメラの速度が最小速度を超えている場合のみ移動
	// 【C言語テクニック】マンハッタン距離（|x|+|y|）を使った簡易的な速度チェック
	if (fabsf(cam->vel.x) + fabsf(cam->vel.y) > cam->min_vel) {
		// 現在のフレーム時間に応じたカメラ移動を適用
		engine.viewport = vec2_add(engine.viewport, vec2_mulf(cam->vel, engine.tick));
	}
}


// カメラを指定位置に即座に設定する関数
void camera_set(camera_t *cam, vec2_t pos) {
	// カメラ位置を設定
	cam->pos = pos;
	// エンジンのビューポートを即座に更新（瞬間移動）
	engine.viewport = camera_viewport_target(cam);
}

// カメラの目標位置を設定する関数（徐々に移動）
void camera_move(camera_t *cam, vec2_t pos) {
	// カメラの目標位置を設定するだけ
	// （実際の移動はcamera_update()で行われる）
	cam->pos = pos;
}

// エンティティをカメラで追跡する関数
void camera_follow(camera_t *cam, entity_ref_t follow, bool snap) {
	// 追跡対象を設定
	cam->follow = follow;
	
	// snapフラグが有効な場合、即座にエンティティに移動
	// 【C言語テクニック】条件付き初期化パターン
	if (snap) {
		// カメラ位置を更新
		camera_update(cam);
		// ビューポートを即座に設定
		engine.viewport = camera_viewport_target(cam);
	}
	// snapフラグが無効の場合は徐々に移動（次回update時）
}

// カメラの追跡を解除する関数
void camera_unfollow(camera_t *cam) {
	// 追跡対象をnone（無効値）に設定
	// 【C言語テクニック】「無効値」パターンの使用例
	cam->follow = entity_ref_none();
}