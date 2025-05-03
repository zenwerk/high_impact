#ifndef HI_ENTITY_H
#define HI_ENTITY_H

// ゲーム内の全ての動的オブジェクトは「エンティティ」です。エンティティは
// フレームごとに1回更新され、描画されます。エンティティのデフォルト関数を
// 独自の実装で上書きすることができます。

// !! このヘッダーをインクルードする前に、entity_t構造体（ENTITY_DEFINE()
// マクロを使用して）、entity_message_t列挙型、およびすべてのエンティティ
// タイプを含むENTITY_TYPES() X-Macroを定義する必要があります。

// ENTITY_DEFINE()で使用される基本構造体については、entity_def.hを参照してください。

#include "types.h"
#include "trace.h"
#include "entity_def.h"
#include "../libs/pl_json.h"


// 必要な#definesまたはtypedefが定義されていない場合、
// コンパイルエラーをトリガーします。

#ifndef ENTITY_TYPES
	#error "#define the X-macro ENTITY_TYPES() before including entity.h"
#endif

// 型が定義されているかを確認するためのコンパイル時チェック
struct _entity_assert_types_defined {
	entity_t CALLL_ENTITY__DEFINE_BEFORE__INCLUDING_ENTITY_H;
	entity_message_t DEFINE_ENUM__entity_message_t__BEFORE_INCLUDING_ENTITY_H;
};



// The maximum amount of entities that are in your game at once. Beyond that,
// entity_spawn() will return NULL.
#if !defined(ENTITIES_MAX)
	#define ENTITIES_MAX 1024
#endif

// The maximum size any of your entities is expected to have. This only affects
// the accuracy of entities_by_proximity() and entities_by_location().
// FIXME: this is bad; we should have to specify this.
#if !defined(ENTITY_MAX_SIZE)
	#define ENTITY_MAX_SIZE 64.0
#endif

// The minimum velocity of an entities (that has restitution > 0) for it to
// bounce. If this would be 0.0, entities would bounce indefinitely with ever
// smaller velocities.
#if !defined(ENTITY_MIN_BOUNCE_VELOCITY)
	#define ENTITY_MIN_BOUNCE_VELOCITY 10.0
#endif

// The axis (x or y) on which we want to do the broad phase collision detection
// sweep & prune. For mosly horizontal games it should be x, for vertical ones y
#if !defined(ENTITY_SWEEP_AXIS)
	#define ENTITY_SWEEP_AXIS x
#endif

// entity_vtab_t構造体はすべてのエンティティタイプで実装する必要があります。
// 各エンティティタイプのために呼び出す関数を保持します。これらはすべてオプションです。
// 最も単純な場合、次のようなグローバル変数を宣言するだけです：
// entity_vtab_t entity_vtab_mytype = {};
typedef struct {
	// プログラム開始時に一度だけ呼び出されます（main_init()の直前）。
	// エンティティタイプのアセットとアニメーションをロードするために使用します。
	void (*load)(void);

	// エンティティがentity_spawn()を通じて作成されるとき、
	// 各エンティティに対して一度だけ呼び出されます。
	// エンティティのすべてのプロパティ（サイズ、オフセット、アニメーション）を
	// 設定するために使用します。
	void (*init)(entity_t *self);

	// engine_load_level()の後、すべてのエンティティが生成された後に一度だけ
	// 呼び出されます。json_t *defにはレベルJSONからの「settings」が含まれています。
	void (*settings)(entity_t *self, json_t *def);

	// フレームごとに各エンティティに対して一度だけ呼び出されます。
	// デフォルトのentity_update_base()は物理特性に従ってエンティティを移動させます。
	void (*update)(entity_t *self);

	// フレームごとに各エンティティに対して一度だけ呼び出されます。
	// デフォルトのentity_draw_base()はentity->animを描画します。
	void (*draw)(entity_t *self, vec2_t viewport);

	// エンティティがentity_kill()を通じてゲームから削除されるときに呼び出されます。
	void (*kill)(entity_t *self);

	// entity->check_againstに従って、このエンティティが他のエンティティに
	// 接触したときに呼び出されます。
	void (*touch)(entity_t *self, entity_t *other);

	// エンティティがゲーム世界または他のエンティティと衝突したときに呼び出されます。
	// 注意：traceはゲーム世界との衝突からのみ設定されます。他のエンティティとの
	// 衝突の場合はNULLになります。
	void (*collide)(entity_t *self, vec2_t normal, trace_t *trace);

	// entity_damage()を通じて呼び出されます。デフォルトのentity_base_damage()は
	// エンティティのヘルスからダメージを差し引き、ヘルスが0以下になると
	// entity_kill()を呼び出します。
	void (*damage)(entity_t *self, entity_t *other, float damage);

	// entity_trigger()を通じて呼び出されます
	void (*trigger)(entity_t *self, entity_t *other);

	// entity_message()を通じて呼び出されます
	void (*message)(entity_t *self, entity_message_t message, void *data);
} entity_vtab_t;


// vtabに従って各エンティティに対して正しい関数を呼び出すマクロ
// 【C言語テクニック】これはオブジェクト指向のポリモーフィズムをC言語で実現する方法です。
// vtabは実質的な仮想関数テーブルとして機能し、エンティティタイプに応じた動作を提供します。
extern entity_vtab_t entity_vtab[ENTITY_TYPES_COUNT];

// エンティティが指定したタイプかどうかをチェック
#define entity_is_type(SELF, TYPE)            (SELF && SELF->type == TYPE)

// エンティティの初期化関数を呼び出し
#define entity_init(ENTITY)                   entity_vtab[ENTITY->type].init(ENTITY)

// エンティティの設定関数を呼び出し（JSONからのデータ設定）
#define entity_settings(ENTITY, DESC)         entity_vtab[ENTITY->type].settings(ENTITY, DESC)

// エンティティの更新関数を呼び出し（毎フレーム実行）
#define entity_update(ENTITY)                 entity_vtab[ENTITY->type].update(ENTITY)

// エンティティの描画関数を呼び出し
#define entity_draw(ENTITY, VIEWPORT)         entity_vtab[ENTITY->type].draw(ENTITY, VIEWPORT)

// エンティティを削除（is_aliveをfalseに設定し、kill関数を呼び出し）
// 【C言語テクニック】カンマ演算子を使い、1行で複数の操作を行っています
#define entity_kill(ENTITY)                   (ENTITY->is_alive = false, entity_vtab[ENTITY->type].kill(ENTITY))

// エンティティが他のエンティティに触れたときの関数を呼び出し
#define entity_touch(ENTITY, OTHER)           entity_vtab[ENTITY->type].touch(ENTITY, OTHER)

// エンティティがコリジョンした際の関数を呼び出し
#define entity_collide(ENTITY, NORMAL, TRACE) entity_vtab[ENTITY->type].collide(ENTITY, NORMAL, TRACE)

// エンティティへのダメージ関数を呼び出し
#define entity_damage(ENTITY, OTHER, DAMAGE)  entity_vtab[ENTITY->type].damage(ENTITY, OTHER, DAMAGE)

// エンティティをトリガーする関数を呼び出し
#define entity_trigger(ENTITY, OTHER)         entity_vtab[ENTITY->type].trigger(ENTITY, OTHER)

// エンティティにメッセージを送信する関数を呼び出し
#define entity_message(ENTITY, MESSAGE, DATA) entity_vtab[ENTITY->type].message(ENTITY, MESSAGE, DATA)

// 指定されたエンティティへの参照を返す
// 【C言語テクニック】ポインタとは別のエンティティ参照方式を提供（安全なハンドル）
entity_ref_t entity_ref(entity_t *self);

// 参照からエンティティを取得。参照されたエンティティがもう有効でない場合はNULLを返す
// 【C言語テクニック】参照からオブジェクトを安全に取得する方法
entity_t *entity_by_ref(entity_ref_t ref);

// 指定されたタイプのエンティティを指定された位置に生成する
// エンティティストレージがいっぱいの場合はNULLを返す
entity_t *entity_spawn(entity_type_t type, vec2_t pos);

// タイプ名からタイプの列挙型を取得
entity_type_t entity_type_by_name(char *type_name);

// タイプの列挙型からタイプ名を取得
const char *entity_type_name(entity_type_t type);

// エンティティの中心位置を取得（posとsizeに基づく）
vec2_t entity_center(entity_t *ent);

// 2つのエンティティ間の距離（ピクセル単位）
float entity_dist(entity_t *a, entity_t *b);

// 2つのエンティティ間の線のラジアン角度
float entity_angle(entity_t *a, entity_t *b); 

// エンティティの位置と速度を物理に従って更新する
// ゲーム世界との衝突もチェックする。vtabでupdate()を使用する場合でも、
// この関数を呼び出すことをお勧めします。
void entity_base_update(entity_t *self);

// entity->animを描画する。vtabでdraw()を使用する場合でも、
// この関数を呼び出すことをお勧めします。
void entity_base_draw(entity_t *self, vec2_t viewport);

// ヘルスからダメージを差し引く; ヘルスが0以下になったらentity_kill()を呼び出す
// vtabでdamage()を使用する場合でも、この関数を呼び出すことをお勧めします。
void entity_base_damage(entity_t *self, entity_t *other, float damage);

// エンティティの名前を取得する（通常、名前はレベルJSONの"settings"で指定される）
// NULLの場合があります。
// 【C言語テクニック】同じ関数名でも引数が異なるため、異なる機能を提供しています
entity_t *entity_by_name(char *name);

// このエンティティの半径内にあるエンティティのリストを取得する
// オプションで特定のエンティティタイプでフィルタリングできる
// すべての近接エンティティを取得するにはENTITY_TYPE_NONEを使用
// ゲーム実行中に呼び出された場合（シーン初期化中ではなく）、
// リストは現在のフレームの間だけ有効
// 【C言語テクニック】一時メモリを使用した高速検索結果の実装
entity_list_t entities_by_proximity(entity_t *ent, float radius, entity_type_t type);

// entities_by_proximity()と同様だが、エンティティの代わりに中心位置を使用
// ゲーム実行中に呼び出された場合（シーン初期化中ではなく）、
// リストは現在のフレームの間だけ有効
entity_list_t entities_by_location(vec2_t pos, float radius, entity_type_t type, entity_t *exclude);

// 特定のタイプのすべてのエンティティのリストを取得
// ゲーム実行中に呼び出された場合（シーン初期化中ではなく）、
// リストは現在のフレームの間だけ有効
entity_list_t entities_by_type(entity_type_t type);

// json_tの配列またはオブジェクトの名前でエンティティのリストを取得
// ゲーム実行中に呼び出された場合（シーン初期化中ではなく）、
// リストは現在のフレームの間だけ有効
entity_list_t entities_from_json_names(json_t *targets);

// 2つのエンティティが重なっているかどうか
// 【C言語テクニック】bool戻り値を使った簡潔な状態チェック関数
bool entity_is_touching(entity_t *self, entity_t *other);


// 以下の関数はシーンの初期化/更新/クリーンアップ中にエンジンによって呼び出されます
// 【C言語テクニック】サブシステムの初期化/終了関数のペア
void entities_init(void);    // エンティティシステムを初期化
void entities_cleanup(void); // エンティティシステムを終了
void entities_reset(void);   // 全エンティティをリセット（シーン切替時など）
void entities_update(void);  // 全エンティティを更新（毎フレーム）
void entities_draw(vec2_t viewport);  // 全エンティティを描画

#endif