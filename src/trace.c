#include "trace.h"
#include "alloc.h"
#include "utils.h"

// 傾斜定義構造体
// 【C言語テクニック】タイルの傾斜情報をコンパクトに表現
typedef struct {
	vec2_t start;   // 傾斜の開始点（正規化座標、0〜1）
	vec2_t dir;     // 傾斜の方向ベクトル
	vec2_t normal;  // 傾斜面の法線ベクトル（衝突応答に使用）
	bool solid;     // 固体か一方通行か
} slope_def_t;

// すべての傾斜タイルを正規化された（0〜1）空間での開始点(x,y)と終了点(x,y)で定義します。
// 傾斜の方向と傾斜の法線ベクトルはこれから計算されます。
// 【C言語テクニック】プリプロセッサマクロによる計算の自動化

// 一部のコンパイラは定数初期化子でsqrt()の使用を許可しますが、
// 他のコンパイラは許可しません。そのため、マクロを使用して平方根を計算します。
// ニュートン法の3回の反復で、必要な0〜1の範囲で十分な精度が得られます。
// 【C言語テクニック】コンパイル時に計算する数学関数マクロ
#define SQRT_ITER(N, GUESS) ((GUESS + ((N) / (GUESS))) * 0.5)
#define SQRT(N) (SQRT_ITER((N), SQRT_ITER((N), SQRT_ITER((N), (N)))))

// ベクトルの長さと法線を計算するマクロ
#define SLOPE_LEN(X, Y) (SQRT((X) * (X) + (Y) * (Y)))
#define SLOPE_NORMAL(X, Y) {((Y) / SLOPE_LEN((X), (Y))), (-(X) / SLOPE_LEN((X), (Y)))}

// 傾斜タイル定義マクロ
// SX,SY: 開始点、EX,EY: 終了点、SOLID: 固体かどうか
#define SLOPE(SX, SY, EX, EY, SOLID) { \
	.start = {SX, SY},\
	.dir = {EX - SX, EY - SY}, \
	.normal = SLOPE_NORMAL(EX - SX, EY - SY), \
	.solid = SOLID \
}

// すべての傾斜タイルの角点は0.0、1.0、0.5、0.333、または0.666のいずれかです。
// ここでH、N、Mとして定義することで、読みやすくなります。
// 【C言語テクニック】定数に意味のある名前を付けて可読性を向上

#define H (1.0 / 2.0)  // 1/2 = 0.5
#define N (1.0 / 3.0)  // 1/3 ≈ 0.333
#define M (2.0 / 3.0)  // 2/3 ≈ 0.666
#define SOLID true     // 固体（通常の壁）
#define ONE_WAY false  // 一方通行（片側からのみ衝突）

// 傾斜タイルの定義配列
// 【C言語テクニック】指定インデックスによる疎配列の初期化
static const slope_def_t slope_definitions[] = {
	// 角度と方向（NE=北東、SE=南東、NW=北西、SW=南西）で分類
	// 傾斜角度は15°、22°、45°、67°、75°
	
	/*     15° NE */ [ 5] = SLOPE(0,1, 1,M, SOLID), [ 6] = SLOPE(0,M, 1,N, SOLID), [ 7] = SLOPE(0,N, 1,0, SOLID),
	/*     22° NE */ [ 3] = SLOPE(0,1, 1,H, SOLID), [ 4] = SLOPE(0,H, 1,0, SOLID),
	/*     45° NE */ [ 2] = SLOPE(0,1, 1,0, SOLID),
	/*     67° NE */ [10] = SLOPE(H,1, 1,0, SOLID), [21] = SLOPE(0,1, H,0, SOLID),
	/*     75° NE */ [32] = SLOPE(M,1, 1,0, SOLID), [43] = SLOPE(N,1, M,0, SOLID), [54] = SLOPE(0,1, N,0, SOLID),
	
	/*     15° SE */ [27] = SLOPE(0,0, 1,N, SOLID), [28] = SLOPE(0,N, 1,M, SOLID), [29] = SLOPE(0,M, 1,1, SOLID),
	/*     22° SE */ [25] = SLOPE(0,0, 1,H, SOLID), [26] = SLOPE(0,H, 1,1, SOLID),
	/*     45° SE */ [24] = SLOPE(0,0, 1,1, SOLID),
	/*     67° SE */ [11] = SLOPE(0,0, H,1, SOLID), [22] = SLOPE(H,0, 1,1, SOLID),
	/*     75° SE */ [33] = SLOPE(0,0, N,1, SOLID), [44] = SLOPE(N,0, M,1, SOLID), [55] = SLOPE(M,0, 1,1, SOLID),
	
	/*     15° NW */ [16] = SLOPE(1,N, 0,0, SOLID), [17] = SLOPE(1,M, 0,N, SOLID), [18] = SLOPE(1,1, 0,M, SOLID),
	/*     22° NW */ [14] = SLOPE(1,H, 0,0, SOLID), [15] = SLOPE(1,1, 0,H, SOLID),
	/*     45° NW */ [13] = SLOPE(1,1, 0,0, SOLID),
	/*     67° NW */ [ 8] = SLOPE(H,1, 0,0, SOLID), [19] = SLOPE(1,1, H,0, SOLID),
	/*     75° NW */ [30] = SLOPE(N,1, 0,0, SOLID), [41] = SLOPE(M,1, N,0, SOLID), [52] = SLOPE(1,1, M,0, SOLID),
	
	/*     15° SW */ [38] = SLOPE(1,M, 0,1, SOLID), [39] = SLOPE(1,N, 0,M, SOLID), [40] = SLOPE(1,0, 0,N, SOLID),
	/*     22° SW */ [36] = SLOPE(1,H, 0,1, SOLID), [37] = SLOPE(1,0, 0,H, SOLID),
	/*     45° SW */ [35] = SLOPE(1,0, 0,1, SOLID),
	/*     67° SW */ [ 9] = SLOPE(1,0, H,1, SOLID), [20] = SLOPE(H,0, 0,1, SOLID),
	/*     75° SW */ [31] = SLOPE(1,0, M,1, SOLID), [42] = SLOPE(M,0, N,1, SOLID), [53] = SLOPE(N,0, 0,1, SOLID),
	
	// 一方通行プラットフォーム（特定の方向からのみ衝突）
	/* 北向き一方通行 */ [12] = SLOPE(0,0, 1,0, ONE_WAY),
	/* 南向き一方通行 */ [23] = SLOPE(1,1, 0,1, ONE_WAY),
	/* 東向き一方通行 */ [34] = SLOPE(1,0, 1,1, ONE_WAY),
	/* 西向き一方通行 */ [45] = SLOPE(0,1, 0,0, ONE_WAY)
};


// 内部関数のプロトタイプ宣言
// タイルをチェックする関数（インライン化して呼び出しオーバーヘッドを削減）
// 【C言語テクニック】static inlineによる最適化
static inline void check_tile(map_t *map, vec2_t pos, vec2_t vel, vec2_t size, vec2i_t tile_pos, trace_t *res);

// 完全なソリッドタイル（ID:1）との衝突解決
static void resolve_full_tile(map_t *map, vec2_t pos, vec2_t vel, vec2_t size, vec2i_t tile_pos, trace_t *res);

// 傾斜タイルとの衝突解決
static void resolve_sloped_tile(map_t *map, vec2_t pos, vec2_t vel, vec2_t size, vec2i_t tile_pos, uint32_t tile, trace_t *res);

// メインのトレース（衝突検出）関数
// マップ上で移動するAABB（軸並行境界ボックス）の衝突を検出し、
// 最初の衝突点と衝突情報を返します
// 【C言語テクニック】スイープ＆プルーン法による連続衝突検出
trace_t trace(map_t *map, vec2_t from, vec2_t vel, vec2_t size) {
	// 移動の終点を計算
	vec2_t to = vec2_add(from, vel);

	// 結果構造体の初期化（デフォルトでは衝突なし）
	trace_t res = {
		.tile = 0,          // 衝突タイル（0 = 衝突なし）
		.pos = to,          // 目標位置（衝突がなければ終点）
		.normal = vec2(0, 0), // 法線ベクトル（衝突なしの場合はゼロ）
		.length = 1         // 正規化された移動距離（1 = 全行程）
	};

	// トレース全体が境界外かどうかの高速チェック
	// 【C言語テクニック】早期リターンによる最適化
	vec2i_t map_size_px = vec2i_muli(map->size, map->tile_size);
	if (
		// 左端が完全にマップ外
		(from.x + size.x < 0 && to.x + size.x < 0) ||
		// 上端が完全にマップ外
		(from.y + size.y < 0 && to.y + size.y < 0) ||
		// 右端が完全にマップ外
		(from.x > map_size_px.x && to.x > map_size_px.x) ||
		// 下端が完全にマップ外
		(from.y > map_size_px.y && to.y > map_size_px.y) ||
		// 移動していない
		(vel.x == 0 && vel.y == 0)
	) {
		return res;
	}

	// 移動方向に基づいてAABBのどの角を使用するかを決定
	// 【C言語テクニック】移動方向を考慮した最適な衝突点選択
	vec2_t offset = vec2(
		vel.x > 0 ? 1 : 0,  // 右に移動→右端、左に移動→左端
		vel.y > 0 ? 1 : 0   // 下に移動→下端、上に移動→上端
	);
	// 選択した角の位置を計算
	vec2_t corner = vec2_add(from, vec2_mul(size, offset));
	// 移動方向に基づく方向ベクトル（タイルチェック用）
	vec2_t dir = vec2_add(vec2_mulf(offset, -2), vec2(1, 1));

	// 必要なステップ数を計算（タイルサイズで正規化）
	// 【C言語テクニック】離散化による連続衝突の近似
	float max_vel = max(vel.x * -dir.x, vel.y * -dir.y);
	int steps = ceil(max_vel / (float)map->tile_size);
	if (steps == 0) {
		return res;
	}
	// 各ステップのサイズを計算
	vec2_t step_size = vec2_divf(vel, steps);

	// 前回チェックしたタイル位置を保存（最適化用）
	vec2i_t last_tile_pos = vec2i(-16, -16);  // 無効な初期値
	
	// 傾斜タイル処理用のフラグ
	bool extra_step_for_slope = false;
	
	// ステップごとにタイルをチェック
	for (int i = 0; i <= steps; i++) {
		// 現在のステップでの角のタイル位置を計算
		vec2i_t tile_pos = vec2i_from_vec2(vec2_divf(vec2_add(corner, vec2_mulf(step_size, i)), map->tile_size));
		
		int corner_tile_checked = 0;
		// X位置が変わった場合、Y方向のタイルをチェック
		if (last_tile_pos.x != tile_pos.x) {
			// Y方向にチェックする必要があるタイル数を計算
			// これはオブジェクトの垂直エッジ（高さ）に沿って
			// 現在のtile_pos.x, tile_pos.y位置からチェックします
			float max_y = from.y + size.y * (1 - offset.y);
			if (i > 0) {
				// X境界を超えた場合のY位置を補間
				max_y += (vel.y / vel.x) * ((tile_pos.x + 1 - offset.x) * map->tile_size - corner.x);
			}

			// 必要なタイル数を計算（小数点以上に切り上げ）
			int num_tiles = ceilf(fabsf(max_y / map->tile_size - tile_pos.y - offset.y));
			for (int t = 0; t < num_tiles; t++) {
				// 各タイルをチェック
				check_tile(map, from, vel, size, vec2i(tile_pos.x, tile_pos.y + dir.y * t), &res);
			}

			last_tile_pos.x = tile_pos.x;
			corner_tile_checked = 1;  // 角のタイルはチェック済み
		}

		// Y位置が変わった場合、X方向のタイルをチェック
		if (last_tile_pos.y != tile_pos.y) {
			// X方向にチェックする必要があるタイル数を計算
			// これはオブジェクトの水平エッジ（幅）に沿って
			// 現在のtile_pos.x, tile_pos.y位置からチェックします
			float max_x = from.x + size.x * (1 - offset.x);
			if (i > 0) {
				// Y境界を超えた場合のX位置を補間
				max_x += (vel.x / vel.y) * ((tile_pos.y + 1 - offset.y) * map->tile_size - corner.y);
			}
			
			// 必要なタイル数を計算（小数点以上に切り上げ）
			int num_tiles = ceilf(fabsf(max_x / map->tile_size - tile_pos.x - offset.x));
			// 角のタイルが既にチェック済みの場合はスキップ
			for (int t = corner_tile_checked; t < num_tiles; t++) {
				check_tile(map, from, vel, size, vec2i(tile_pos.x + dir.x * t, tile_pos.y), &res);
			}

			last_tile_pos.y = tile_pos.y;
		}

		// 傾斜タイルとの衝突処理の特別ケース
		// 【C言語テクニック】特殊ケースの効率的なハンドリング
		// 傾斜タイルと衝突した場合、もう1ステップ先をチェックする必要がある
		// 別のタイルにより早いポイントで衝突する可能性があるため
		// 完全なソリッドタイル（ID: 1）の場合はここで戻れる
		if (res.tile > 0 && (res.tile == 1 || extra_step_for_slope)) {
			return res;
		}
		extra_step_for_slope = true;
	}

	// 衝突がなかった場合、または全てのステップをチェックした後の結果を返す
	return res;	
}

// タイルのタイプをチェックして適切な衝突解決関数に振り分ける
// 【C言語テクニック】インライン関数による分岐の最適化
static inline void check_tile(map_t *map, vec2_t pos, vec2_t vel, vec2_t size, vec2i_t tile_pos, trace_t *res) {
	// タイル位置でタイルIDを取得
	uint32_t tile = map_tile_at(map, tile_pos);
	
	// タイルタイプに基づいて処理を分岐
	if (tile == 0) {
		return;  // 空のタイル（衝突なし）
	}
	else if (tile == 1) {
		// 完全なソリッドタイル（ID:1）
		resolve_full_tile(map, pos, vel, size, tile_pos, res);
	}
	else {
		// 傾斜タイルやその他の特殊タイル
		resolve_sloped_tile(map, pos, vel, size, tile_pos, tile, res);
	}
}

// 完全なソリッドタイル（ID:1）との衝突を解決する
// 【C言語テクニック】AABB vs. AABBの衝突検出と解決
static void resolve_full_tile(map_t *map, vec2_t pos, vec2_t vel, vec2_t size, vec2i_t tile_pos, trace_t *res) {
	// 衝突が発生した場合の最小結果位置（x または y）を計算
	// x または y 座標のどちらか一方のみが正しい値になります
	// （水平または垂直に侵入するかによって異なる）
	// 間違っている座標は後で再計算します

	// タイル位置をピクセル座標に変換し、移動方向に基づいて衝突位置を計算
	vec2_t rp = vec2_add(
		vec2_from_vec2i(vec2i_muli(tile_pos, map->tile_size)),
		vec2(
			(vel.x > 0 ? -size.x : map->tile_size),  // 右移動時：左端、左移動時：右端
			(vel.y > 0 ? -size.y : map->tile_size)   // 下移動時：上端、上移動時：下端
		)
	);

	float length = 1;  // 正規化された移動距離

	// 衝突の方向（水平か垂直か）を判定
	// Y方向に移動していない場合、または移動ベクトルとタイル角の外積の符号が正しい場合は
	// 水平衝突、そうでなければ垂直衝突
	// 【C言語テクニック】ベクトルの外積（クロス積）を使った衝突方向判定
	float sign = (vel.x * (rp.y - pos.y) - vel.y * (rp.x - pos.x)) * vel.x * vel.y;

	if (sign < 0 || vel.y == 0) {
		// 水平方向の衝突（左右の辺）
		// 衝突までの正規化された距離を計算
		length = fabsf((pos.x - rp.x) / vel.x);
		// 既に別のタイルとより早く衝突していたら終了
	 	if (length > res->length) {
	 		return;
	 	};
		
		// 衝突点のY座標を補間
		rp.y = pos.y + length * vel.y;
		// 衝突面の法線ベクトルを設定（左右の壁）
		res->normal = vec2((vel.x > 0 ? -1 : 1), 0);
	}
	else {
		// 垂直方向の衝突（上下の辺）
		// 衝突までの正規化された距離を計算
		length = fabsf((pos.y - rp.y) / vel.y);
		// 既に別のタイルとより早く衝突していたら終了
		if (length > res->length) {
			return;
		};
		
		// 衝突点のX座標を補間
		rp.x = pos.x + length * vel.x;
		// 衝突面の法線ベクトルを設定（床や天井）
		res->normal = vec2(0, (vel.y > 0 ? -1 : 1));
	}

	// 結果を更新
	res->tile = 1;
	res->tile_pos = tile_pos;
	res->length = length;
	res->pos = rp;
}

// 傾斜タイルとの衝突を解決する
// 【C言語テクニック】線分 vs. 線分の衝突判定
static void resolve_sloped_tile(map_t *map, vec2_t pos, vec2_t vel, vec2_t size, vec2i_t tile_pos, uint32_t tile, trace_t *res) {
	// 有効な傾斜タイル定義が存在するか確認
	if (tile < 2 || tile >= len(slope_definitions)) {
		return;
	}

	// タイルIDから傾斜定義を取得
	const slope_def_t *slope = &slope_definitions[tile];

	// 傾斜線の開始点と方向をワールド座標に変換
	// 【C言語テクニック】タイル空間からピクセル空間への変換
	vec2_t tile_pos_px = vec2_mulf(vec2_from_vec2i(tile_pos), map->tile_size);

	// 傾斜線の開始点と方向をタイルサイズでスケーリング
	vec2_t ss = vec2_mulf(slope->start, map->tile_size);
	vec2_t sd = vec2_mulf(slope->dir, map->tile_size);
	// オブジェクトの位置をタイルローカル座標に変換
	vec2_t local_pos = vec2_sub(pos, tile_pos_px);

	// オブジェクトの移動ベクトルと傾斜線との線分 vs. 線分の衝突判定
	// 【C言語テクニック】線分の交差判定アルゴリズム
	// 注意: 精度の問題がまだある。傾斜に沿ってとても遅く移動している場合、
	// 傾斜の裏側に滑り込む可能性があります。
	// TODO: 傾斜タイルを3つの無限線で定義された三角形として扱う方が
	// 良いかもしれません。点がその中にあるかどうかを素早くチェックできますが、
	// どの側から三角形に移動しているかを判断する必要があります。

	// わずかな数値誤差を許容するためのイプシロン値
	const float epsilon = 0.001;
	
	// 速度ベクトルと傾斜方向ベクトルの外積（determinant）
	float determinant = vec2_cross(vel, sd);

	// 傾斜線に対する衝突判定（determinantが負の場合は潜在的に衝突）
	if (determinant < -epsilon) {
		// 角の位置を計算（移動方向に基づく）
		vec2_t corner = vec2_add(
			vec2_sub(local_pos, ss),
			vec2(sd.y < 0 ? size.x : 0, sd.x > 0 ? size.y : 0)
		);

		// クラメールの法則を使用して線分の交点を計算
		float point_at_slope = vec2_cross(vel, corner) / determinant;
		float point_at_vel = vec2_cross(sd, corner) / determinant;
		
		// 傾斜の前にいて、それに向かって移動しているか？
		if (
			point_at_vel > -epsilon &&
			point_at_vel < 1 + epsilon &&
			point_at_slope > -epsilon &&
			point_at_slope < 1 + epsilon
		) {
			// これは既に衝突したポイントよりも早いポイントか？
			if (point_at_vel <= res->length) {
				// 結果を更新
				res->tile = tile;
				res->tile_pos = tile_pos;
				res->length = point_at_vel;
				res->normal = slope->normal;
				res->pos = vec2_add(pos, vec2_mulf(vel, point_at_vel));
			}
			return;
		}
	}
	
	// これは非ソリッド（一方通行）タイルで、間違った側から来ているか？
	if (!slope->solid && (determinant > 0 || sd.x * sd.y != 0)) {
		return;
	}

	// 傾斜自体との衝突はなかったが、まだ傾斜の角や
	// タイルの残りの辺との衝突をチェックする必要がある
	// 【C言語テクニック】エッジケースの詳細な処理

	// 水平または垂直の衝突に対する潜在的な衝突点を計算し、
	// タイルとまだ衝突する最小および最大座標を計算します

	vec2_t rp;       // 結果位置
	vec2_t min;      // 最小境界座標
	vec2_t max;      // 最大境界座標
	float length = 1;  // 正規化された衝突距離

	// 傾斜の方向に基づいて境界を計算
	if (sd.y >= 0) {
		// 左タイルエッジ
		min.x = -size.x - epsilon;

		// 左または右の傾斜角？
		max.x = (vel.y > 0 ? ss.x : ss.x + sd.x) - epsilon;
		rp.x = vel.x > 0 ? min.x : max(ss.x, ss.x + sd.x);
	}
	else {
		// 左または右の傾斜角？
		min.x = (vel.y > 0 ? ss.x + sd.x : ss.x) - size.x + epsilon;

		// 右タイルエッジ
		max.x = map->tile_size + epsilon; 
		rp.x = vel.x > 0 ? min(ss.x, ss.x + sd.x) - size.x : max.x;
	}

	if (sd.x > 0) {
		// 上または下の傾斜角？
		min.y = (vel.x > 0 ? ss.y : ss.y + sd.y) - size.y + epsilon;

		// 下タイルエッジ
		max.y = map->tile_size + epsilon;
		rp.y = vel.y > 0 ? min(ss.y, ss.y + sd.y) - size.y : max.y;
	}
	else {
		// 上タイルエッジ
		min.y = -size.y - epsilon;

		// 上または下の傾斜角？
		max.y = (vel.x > 0 ? ss.y + sd.y : ss.y) - epsilon;
		rp.y = vel.y > 0 ? min.y : max(ss.y, ss.y + sd.y);
	}

	// これが水平または垂直の衝突かどうかを判断
	// このステップは完全なタイル衝突と同様です
	float sign = vec2_cross(vel, vec2_sub(rp, local_pos)) * vel.x * vel.y;
	
	if (sign < 0 || vel.y == 0) {
		// 水平方向の衝突（X方向、左または右のエッジ）
		length = fabsf((local_pos.x - rp.x) / vel.x);
		rp.y = local_pos.y + length * vel.y;
		
		// 衝突が無効または別の衝突が優先される場合
		if (
			rp.y >= max.y || rp.y <= min.y ||  // Y範囲外
			length > res->length ||             // 既に衝突あり
			(!slope->solid && sd.y == 0)        // 一方通行かつ水平
		) {
			return;
		}

		// 水平衝突の法線を設定
		res->normal.x = (vel.x > 0 ? -1 : 1);
		res->normal.y = 0;
	}
	else {
		// 垂直方向の衝突（Y方向、上または下のエッジ）			
		length = fabsf((local_pos.y - rp.y) / vel.y);
		rp.x = local_pos.x + length * vel.x;

		// 衝突が無効または別の衝突が優先される場合
		if (
			rp.x >= max.x || rp.x <= min.x ||  // X範囲外
			length > res->length ||             // 既に衝突あり
			(!slope->solid && sd.x == 0)        // 一方通行かつ垂直
		) {
			return;
		}

		// 垂直衝突の法線を設定
		res->normal.x = 0;
		res->normal.y = (vel.y > 0 ? -1 : 1);
	}

	// 有効な衝突が見つかったので結果を更新
	res->tile = tile;
	res->tile_pos = tile_pos;
	res->length = length;
	res->pos = vec2_add(rp, tile_pos_px);  // ローカル→ワールド座標変換
}
