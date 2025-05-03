#ifndef HI_ENTITY_DEF_H
#define HI_ENTITY_DEF_H


// エンティティ参照（Entity Refs）
// エンティティを安全に追跡するために使用できます。参照はentity_by_ref()を使って
// 実際のentity_tに解決できます。参照先のエンティティが既に無効（死亡など）の場合、
// 参照はNULLに解決されます。これにより、直接entity_t*を使用する際のエラーを防ぎます。
// 直接ポインタは常に有効なエンティティストレージを指しますが、それはもはや
// あなたが望んでいたエンティティではないかもしれません。
//
// 【C言語テクニック】これはハンドルパターンの実装例です。
// IDと配列インデックスを組み合わせて、オブジェクトを安全に参照します。
typedef struct {
	uint16_t id;     // エンティティの一意のID
	uint16_t index;  // エンティティ配列内のインデックス
} entity_ref_t;

// エンティティ参照のリスト
// 通常はバンプアロケータで割り当てられるため、現在のフレームでのみ有効です。
typedef struct {
	uint32_t len;            // リスト内のエンティティ数
	entity_ref_t *entities;  // エンティティ参照の配列
} entity_list_t;

// 無効なエンティティ参照を作成するマクロ（常にNULLに解決される）
// 【C言語テクニック】複合リテラルを使った構造体初期化
#define entity_ref_none() (entity_ref_t){.id = 0, .index = 0}

// エンティティグループシステム
// エンティティは1つ以上のグループに所属できます（ent->groupを通じて）。
// これはent->check_againstと組み合わせて使用することで、
// entity_touch()で通知を受けたいエンティティのペアを指定できます。
// グループはOR演算子で結合できます。
//
// 例：以下の2つのエンティティがある場合
//   ent_a->group = ENTITY_GROUP_ITEM | ENTITY_GROUP_BREAKABLE;
//   ent_b->check_against = ENTITY_GROUP_BREAKABLE;
// 関数
//   entity_touch(ent_b, ent_a) 
// は、これら2つのエンティティが重なったときに呼び出されます。
//
// 【C言語テクニック】ビットフラグを使用したグループ分けの実装
// ビットごとのOR操作で複数グループの所属を効率的に表現できます
typedef enum {
	ENTITY_GROUP_NONE =       (0),           // グループなし
	ENTITY_GROUP_PLAYER =     (1 << 0),      // プレイヤーグループ
	ENTITY_GROUP_NPC =        (1 << 1),      // NPCグループ
	ENTITY_GROUP_ENEMY =      (1 << 2),      // 敵グループ
	ENTITY_GROUP_ITEM =       (1 << 3),      // アイテムグループ
	ENTITY_GROUP_PROJECTILE = (1 << 4),      // 発射物グループ
	ENTITY_GROUP_PICKUP =     (1 << 5),      // 拾得物グループ
	ENTITY_GROUP_BREAKABLE =  (1 << 6),      // 破壊可能オブジェクトグループ
} entity_group_t;


// entity_physics_tで使用される衝突モード
// entity_physics_tで必要な設定がない場合は、これらを直接使用することもできます。
// 例えば、エンティティが他のエンティティと衝突するが、collision_mapとは衝突しない
// という設定にしたい場合は、次のようにentity_physics_tを拡張できます：
//   ENTITY_PHYSICS_MOVE | ENTITY_COLLIDES_ACTIVE
//
// 【C言語テクニック】ビットフラグを使用した衝突モードの実装
// これにより、複数の衝突モードを組み合わせて複雑な物理動作を定義できます
typedef enum {
	ENTITY_COLLIDES_WORLD   = (1 << 1),  // ワールド（マップ）との衝突
	ENTITY_COLLIDES_LITE    = (1 << 4),  // 軽量衝突モード
	ENTITY_COLLIDES_PASSIVE = (1 << 5),  // 受動的衝突モード
	ENTITY_COLLIDES_ACTIVE  = (1 << 6),  // 能動的衝突モード
	ENTITY_COLLIDES_FIXED   = (1 << 7),  // 固定衝突モード
} entity_collision_mode_t;

// ent->physicsは、エンティティの移動と衝突の方法を決定します。
// 【C言語テクニック】列挙型と複合ビットフラグを組み合わせた物理動作の定義
typedef enum {
	// 衝突しない、移動しない。単に存在するだけのアイテムに便利です。
	ENTITY_PHYSICS_NONE    = 0,

	// 速度に従ってエンティティを移動させるが、衝突しない
	ENTITY_PHYSICS_MOVE    = (1 << 0),

	// エンティティを移動させ、collision_mapと衝突する
	ENTITY_PHYSICS_WORLD   = ENTITY_PHYSICS_MOVE  | ENTITY_COLLIDES_WORLD,

	// エンティティを移動させ、collision_mapおよび他のエンティティと衝突するが、
	// 一致する物理特性を持つ他のエンティティとのみ衝突します：
	//
	// ACTIVE vs. LITEまたはFIXED vs. ANYの衝突では、「弱い」エンティティのみが
	// 移動し、もう一方は固定されたままです。
	// ACTIVE vs. ACTIVEおよびACTIVE vs. PASSIVEの衝突では、両方のエンティティが
	// 移動します。
	// LITEまたはPASSIVEエンティティは、他のLITEまたはPASSIVEエンティティとは
	// まったく衝突しません。
	// FIXED vs. FIXEDの衝突の挙動は未定義です。
	//
	// 【C言語テクニック】既存の定数を組み合わせて新しい値を作成
	ENTITY_PHYSICS_LITE    = ENTITY_PHYSICS_WORLD | ENTITY_COLLIDES_LITE,    // 軽量物理
	ENTITY_PHYSICS_PASSIVE = ENTITY_PHYSICS_WORLD | ENTITY_COLLIDES_PASSIVE, // 受動的物理
	ENTITY_PHYSICS_ACTIVE  = ENTITY_PHYSICS_WORLD | ENTITY_COLLIDES_ACTIVE,  // 能動的物理
	ENTITY_PHYSICS_FIXED   = ENTITY_PHYSICS_WORLD | ENTITY_COLLIDES_FIXED,   // 固定物理
} entity_physics_t;

// 前方宣言（循環依存を避けるため）
typedef struct entity_t entity_t;

// ENTITY_DEFINE()マクロの説明
// あなたのコードからentity_def.hの後、entity.hをインクルードする前に呼び出されると、
// entity_structを定義します。これはまた、あなたのコードのENTITY_TYPES() Xマクロに
// 従ってENTITY_TYPE_*列挙型も作成します。
// 例：
/*
     #include "entity_def.h"
     ENTITY_TYPES(TYPE) \  
        TYPE(ENTITY_TYPE_PLAYER, player) \  
        TYPE(ENTITY_TYPE_ENEMY, enemy)  
     ENTITY_DEFINE()
     #include "entity.h"
*/
//
// 【C言語テクニック】X-マクロとマクロ展開の高度な使用例
// 型安全なエンティティシステムを自動生成します

// 列挙型の値を宣言するためのヘルパーマクロ
#define ENTITY_DECLARE_TYPE_ENUM(ENUM, NAME) ENUM,

// エンティティ構造体を定義する主要マクロ
// 【C言語テクニック】可変引数マクロを使用して拡張可能な構造体を実現
#define ENTITY_DEFINE(...) \
	/* エンティティタイプの列挙型を定義 */ \
	typedef enum { \
		ENTITY_TYPE_NONE = 0, \
		ENTITY_TYPES(ENTITY_DECLARE_TYPE_ENUM) \
		ENTITY_TYPES_COUNT \
	} entity_type_t; \
	\
	/* エンティティ構造体本体の定義 */ \
	struct entity_t { \
		uint16_t id;             /* このエンティティの一意のID（生成時に割り当て） */ \
		bool is_alive;           /* このエンティティが使用中かどうかを決定 */ \
		bool on_ground;          /* engine.gravity > 0で何かの上に立っている場合はtrue */ \
		int32_t draw_order;      /* エンティティは描画前にこの値で（昇順に）ソートされる */ \
		entity_type_t type;      /* エンティティのタイプ（ENTITY_TYPE_*） */ \
		entity_physics_t physics; /* 物理挙動 */ \
		entity_group_t group;    /* このエンティティが所属するグループ */ \
		entity_group_t check_against; /* このエンティティが触れられるグループ */ \
		vec2_t pos;              /* ゲーム世界内のバウンディングボックスの左上位置（通常は直接操作しない） */ \
		vec2_t size;             /* 物理用のバウンディングボックス */ \
		vec2_t vel;              /* 速度 */ \
		vec2_t accel;            /* 加速度 */ \
		vec2_t friction;         /* engine.tick * velocityの係数としての摩擦 */ \
		vec2_t offset;           /* アニメーションを描画する位置からのオフセット */ \
		char *name;              /* ターゲットなどに使用される名前（通常はJSONデータを通して設定） */ \
		float health;            /* エンティティがダメージを受け、結果的にhealth < 0になると、エンティティは削除される */ \
		float gravity;           /* engine.gravityとの重力係数。デフォルト1.0 */ \
		float mass;              /* アクティブな衝突のための質量係数。デフォルト1.0 */\
		float restitution;       /* 「弾性係数」（跳ね返りの強さ） */ \
		float max_ground_normal; /* 斜面の場合、on_groundフラグを設定するために斜面がどれだけ急であるかを決定。デフォルトはcosf(to_radians(46)) */ \
		float min_slide_normal;  /* 斜面の場合、エンティティが滑り落ちるために斜面がどれだけ急である必要があるかを決定。デフォルトはcosf(to_radians(0)) */ \
		anim_t anim;             /* 自動的に描画されるアニメーション */ \
		__VA_ARGS__              /* ENTITY_DEFINE(...)を通して定義されるあなた独自のプロパティ */ \
	};

// これらのマクロはentities/*.cファイルで使用して、エディタ
// （tools/weltmeister.html）での表示を設定できます
//
// 【C言語テクニック】ダミーマクロの使用
// これらは実際にはエディタツールによって解析されるコメントとして機能し、
// コンパイル時には無視されます
#define EDITOR_SIZE(X, Y)     // エディタでのサイズ。デフォルト(8, 8)
#define EDITOR_RESIZE(RESIZE) // エディタでエンティティのサイズ変更が可能かどうか。デフォルトfalse
#define EDITOR_COLOR(R, G, B) // エディタでのボックス色。デフォルト(128, 255, 128)
#define EDITOR_IGNORE(IGNORE) // このエンティティがエディタで作成可能かどうか。デフォルトfalse

#endif
