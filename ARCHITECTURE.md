# high_impact アーキテクチャドキュメント

## 概要

high_impact は2Dゲーム開発のための軽量なC言語フレームワークです。シーンベースのゲーム構成、エンティティ・コンポーネントシステム、カスタムメモリ管理、そして複数のレンダリングバックエンドをサポートしています。

## システム全体構成

```plantuml
@startuml
package "high_impact Framework" {
	[Game Code] as game

	package "Core Engine" {
		[Engine] as engine
		[Scene Management] as scene
	}

	package "Entity System" {
		[Entity Manager] as entity
		[Entity Types] as entity_types
	}

	package "Rendering System" {
		[Render Core] as render
		[Image] as image
		[Animation] as animation
		[Font] as font
		[Render Backend] as backend
	}

	package "Platform Layer" {
		[Platform Abstraction] as platform
		[Input System] as input
		[Sound System] as sound
	}

	package "Memory Management" {
		[Allocator] as alloc
		[Bump Allocator] as bump
		[Temp Allocator] as temp
	}

	package "World System" {
		[Map/Tilemap] as map
		[Camera] as camera
		[Collision/Trace] as trace
	}

	package "Utilities" {
		[Types] as types
		[Utils] as utils
	}
}

game --> engine
game --> entity
game --> render
game --> input
game --> sound

engine --> scene
engine --> entity
engine --> map
engine --> render

entity --> entity_types
entity --> animation

render --> image
render --> font
render --> backend

platform --> input
platform --> sound
platform --> render

map --> image
map --> trace

camera --> entity

alloc --> bump
alloc --> temp

entity --> alloc
render --> alloc
map --> alloc

trace --> map
entity --> trace

types <-- utils
@enduml
```

## レイヤー構造

```plantuml
@startuml
rectangle "アプリケーション層" #LightBlue {
	rectangle "Game Code\n(ユーザー実装)" as app
}

rectangle "フレームワーク層" #LightGreen {
	rectangle "Engine\n(シーン管理、ゲームループ)" as engine
	rectangle "Entity System\n(エンティティ管理)" as entity
	rectangle "Rendering\n(描画システム)" as render
	rectangle "World\n(マップ、カメラ)" as world
}

rectangle "サブシステム層" #LightYellow {
	rectangle "Memory\n(メモリ管理)" as memory
	rectangle "Input\n(入力抽象化)" as input
	rectangle "Sound\n(音声システム)" as sound
	rectangle "Image/Animation\n(リソース管理)" as resource
}

rectangle "プラットフォーム層" #LightCoral {
	rectangle "Platform Abstraction\n(SDL/Sokol)" as platform
	rectangle "Render Backend\n(OpenGL/Metal/Software)" as backend
}

rectangle "OS/ハードウェア層" #Gray {
	rectangle "Operating System\n(Windows/macOS/Linux/Web)" as os
}

app --> engine
app --> entity
app --> render
app --> world

engine --> memory
engine --> entity
engine --> render
engine --> world

entity --> memory
render --> memory
world --> memory

input --> platform
sound --> platform
backend --> platform
resource --> memory

platform --> os
backend --> os

@enduml
```

## コアモジュール詳細

### 1. Engine (engine.h/c)

エンジンはゲームの中心的なシステムで、ゲームループ、シーン管理、時間管理を担当します。

```plantuml
@startuml
class engine_t {
	+ time_real : double
	+ time : double
	+ time_scale : double
	+ tick : double
	+ frame : uint64_t
	+ collision_map : map_t*
	+ background_maps : map_t*[]
	+ background_maps_len : uint32_t
	+ gravity : float
	+ viewport : vec2_t
	+ perf : struct
}

class scene_t {
	+ init() : void
	+ update() : void
	+ draw() : void
	+ cleanup() : void
}

engine_t "1" --> "*" scene_t : manages
engine_t "1" --> "*" map_t : contains

note right of engine_t
	グローバル変数として
	単一インスタンスが存在
end note

note right of scene_t
	ゲームの各シーン
	(タイトル、ゲーム本編等)
	のライフサイクルを定義
end note
@enduml
```

**主要機能:**
- シーンの切り替え管理 (`engine_set_scene()`)
- レベルの読み込み (`engine_load_level()`)
- 時間管理とフレームカウント
- ビューポート管理
- パフォーマンス統計の収集

### 2. Entity System (entity.h/c, entity_def.h)

エンティティシステムは、ゲーム内の動的オブジェクトを管理します。仮想関数テーブル(vtable)を使用したポリモーフィズムを実現しています。

```plantuml
@startuml
class entity_t {
	+ id : uint16_t
	+ is_alive : bool
	+ on_ground : bool
	+ draw_order : int32_t
	+ type : entity_type_t
	+ physics : entity_physics_t
	+ group : entity_group_t
	+ check_against : entity_group_t
	+ pos : vec2_t
	+ size : vec2_t
	+ vel : vec2_t
	+ accel : vec2_t
	+ friction : vec2_t
	+ offset : vec2_t
	+ name : char*
	+ health : float
	+ gravity : float
	+ mass : float
	+ restitution : float
	+ anim : anim_t
}

class entity_vtab_t {
	+ load() : void
	+ init(entity_t*) : void
	+ settings(entity_t*, json_t*) : void
	+ update(entity_t*) : void
	+ draw(entity_t*, vec2_t) : void
	+ kill(entity_t*) : void
	+ touch(entity_t*, entity_t*) : void
	+ collide(entity_t*, vec2_t, trace_t*) : void
	+ damage(entity_t*, entity_t*, float) : void
	+ trigger(entity_t*, entity_t*) : void
	+ message(entity_t*, entity_message_t, void*) : void
}

class entity_ref_t {
	+ id : uint16_t
	+ index : uint16_t
}

enum entity_physics_t {
	ENTITY_PHYSICS_NONE
	ENTITY_PHYSICS_MOVE
	ENTITY_PHYSICS_WORLD
	ENTITY_PHYSICS_LITE
	ENTITY_PHYSICS_PASSIVE
	ENTITY_PHYSICS_ACTIVE
	ENTITY_PHYSICS_FIXED
}

enum entity_group_t {
	ENTITY_GROUP_NONE
	ENTITY_GROUP_PLAYER
	ENTITY_GROUP_NPC
	ENTITY_GROUP_ENEMY
	ENTITY_GROUP_ITEM
	ENTITY_GROUP_PROJECTILE
	ENTITY_GROUP_PICKUP
	ENTITY_GROUP_BREAKABLE
}

entity_t --> entity_physics_t
entity_t --> entity_group_t
entity_t "1" --> "1" entity_vtab_t : uses
entity_ref_t ..> entity_t : refers to

note right of entity_vtab_t
	各エンティティタイプは
	独自のvtableを持ち、
	タイプごとの動作を定義
end note

note left of entity_ref_t
	安全なエンティティ参照
	(ハンドルパターン)
end note
@enduml
```

**主要機能:**
- エンティティのスポーンと管理
- vtableによる仮想関数システム
- 物理シミュレーション (`entity_base_update()`)
- 衝突検出とグループ管理
- エンティティ検索機能 (近接、タイプ別など)

### 3. Memory Management (alloc.h/c)

カスタムメモリ管理システムで、malloc/freeを使用せず、単一のメモリブロック(ハンク)から割り当てを行います。

```plantuml
@startuml
package "Memory System" {
	class "Hunk (固定サイズメモリブロック)" as hunk {
		合計サイズ: ALLOC_SIZE (デフォルト32MB)
	}

	class "Bump Allocator" as bump {
		+ bump_alloc(size)
		+ bump_mark()
		+ bump_reset(mark)
		+ bump_from_temp(temp, offset, size)
		--
		特徴:
		- 線形に成長
		- マーク&リセットでまとめて解放
		- シーン/フレームのライフタイムに対応
	}

	class "Temp Allocator" as temp {
		+ temp_alloc(size)
		+ temp_free(p)
		+ temp_alloc_check()
		--
		特徴:
		- ハンクの末尾から割り当て
		- 個別に解放可能
		- 短命オブジェクト用
		- フレーム終了時に空であることを確認
	}

	hunk <-- bump : 先頭から使用
	hunk <-- temp : 末尾から使用
}

note bottom of hunk
	メモリレイアウト:
	[Bump →              ← Temp]
	[永続データ | 一時データ]
end note

note right of bump
	使用例:
	- エンティティ
	- マップデータ
	- 画像リソース
	- シーンごとのデータ
end note

note left of temp
	使用例:
	- ファイル読み込み時の一時バッファ
	- 検索結果のリスト
	- JSON解析用バッファ
end note
@enduml
```

**メモリライフタイム管理:**

```plantuml
@startuml
start
:プログラム開始;
:Hunk初期化;
:永続データ割り当て(Bump);

while (ゲーム実行中?) is (yes)
	:シーン開始;
	:Bump位置を記録;
	:シーンデータ割り当て(Bump);

	while (シーン実行中?) is (yes)
		:フレーム開始;
		:Bump位置を記録;

		:フレームデータ割り当て(Bump);
		:一時データ割り当て(Temp);

		:更新処理;
		:描画処理;

		:一時データ解放(Temp);
		:temp_alloc_check();
		:Bumpをフレーム開始位置にリセット;
		:フレーム終了;
	endwhile (no)

	:Bumpをシーン開始位置にリセット;
	:シーン終了;
endwhile (no)

:プログラム終了;
stop
@enduml
```

### 4. Rendering System (render.h/c)

描画システムは複数のバックエンド(OpenGL, Metal, Software)をサポートし、統一されたAPIを提供します。

```plantuml
@startuml
package "Rendering System" {
	interface "Render API" as api {
		+ render_init(size)
		+ render_cleanup()
		+ render_resize(size)
		+ render_push()
		+ render_pop()
		+ render_translate(vec2_t)
		+ render_scale(vec2_t)
		+ render_rotate(float)
		+ render_draw(...)
		+ render_frame_prepare()
		+ render_frame_end()
	}

	class "Render Core\n(render.c)" as core {
		+ トランスフォームスタック管理
		+ 論理解像度の計算
		+ スケール/リサイズモード処理
		+ テクスチャ管理
	}

	class "OpenGL Backend\n(render_gl.c)" as gl {
		+ render_backend_init()
		+ render_backend_cleanup()
		+ render_draw_quad()
		+ texture_create()
		--
		OpenGL 3.3+ / ES 3.0
	}

	class "Metal Backend\n(render_metal.m)" as metal {
		+ render_backend_init()
		+ render_backend_cleanup()
		+ render_draw_quad()
		+ texture_create()
		--
		macOS/iOS向け
	}

	class "Software Backend\n(render_software.c)" as software {
		+ render_backend_init()
		+ render_backend_cleanup()
		+ render_draw_quad()
		+ texture_create()
		--
		CPUレンダリング
	}

	api <|.. core
	core --> gl : uses
	core --> metal : uses
	core --> software : uses
}

class "Image System\n(image.h/c)" as image {
	+ image(path)
	+ image_with_pixels(size, pixels)
	+ image_draw(img, pos)
	+ image_draw_ex(...)
	+ image_draw_tile(...)
}

class "Animation System\n(animation.h/c)" as animation {
	+ anim_def_with_len(...)
	+ anim_rewind(anim)
	+ anim_goto(anim, frame)
	+ anim_draw(anim, pos)
}

class "Font System\n(font.h/c)" as font {
	+ font(path, definition_path)
	+ font_draw(font, pos, text, align)
	+ font_line_width(font, text)
}

api <-- image : uses
api <-- animation : uses
api <-- font : uses

note right of core
	描画の流れ:
	1. render_push()でトランスフォーム保存
	2. render_translate/scale/rotate()で変換
	3. render_draw()で描画
	4. render_pop()で復元
end note
@enduml
```

**レンダリングパイプライン:**

```plantuml
@startuml
start

:フレーム開始;
:render_frame_prepare();

partition "Scene Draw" {
	:render_push();

	:背景マップ描画;
	note right
		distance値に基づく
		パララックススクロール
	end note

	partition "Entity Draw" {
		:エンティティをdraw_orderでソート;

		while (各エンティティ) is (not done)
			:render_push();
			:entity_draw(ent, viewport);
			note right
				内部でanim_draw()や
				image_draw()を呼び出し
			end note
			:render_pop();
		endwhile (done)
	}

	:前景マップ描画;

	:render_pop();
}

:ポストエフェクト適用;
note right
	CRT効果など
end note

:render_frame_end();
:バッファスワップ;

stop
@enduml
```

### 5. Platform Layer (platform.h/c, platform_sdl.c, platform_sokol.c)

プラットフォーム抽象化レイヤーは、異なるバックエンド(SDL, Sokol)への統一インターフェースを提供します。

```plantuml
@startuml
interface "Platform API" as papi {
	+ platform_screen_size()
	+ platform_now()
	+ platform_get_fullscreen()
	+ platform_set_fullscreen(bool)
	+ platform_samplerate()
	+ platform_load_asset(name, bytes_read)
	+ platform_load_asset_json(name)
	+ platform_set_audio_mix_cb(cb)
	+ platform_exit()
}

class "SDL Implementation\n(platform_sdl.c)" as sdl {
	+ SDL_Window管理
	+ SDL_GLContext管理
	+ イベント処理
	+ ファイルI/O
	+ オーディオコールバック
}

class "Sokol Implementation\n(platform_sokol.c)" as sokol {
	+ sokol_app使用
	+ Webブラウザ対応
	+ イベント処理
	+ ファイルI/O
	+ オーディオコールバック
}

papi <|.. sdl
papi <|.. sokol

note right of sdl
	デスクトップ向け:
	- Windows
	- macOS
	- Linux
end note

note right of sokol
	軽量かつWeb対応:
	- WebGL/WASM
	- デスクトップ
end note
@enduml
```

### 6. Input System (input.h/c)

入力システムは、キーボード、マウス、ゲームパッドの入力を抽象化し、アクションベースのバインディングを提供します。

```plantuml
@startuml
class "Input System" as input {
	+ input_bind(button, action)
	+ input_unbind(button)
	+ input_unbind_all()
	+ input_state(action)
	+ input_pressed(action)
	+ input_released(action)
	+ input_mouse_pos()
	+ input_capture(cb, user)
}

enum button_t {
	INPUT_KEY_A..Z
	INPUT_KEY_0..9
	INPUT_GAMEPAD_A..HOME
	INPUT_MOUSE_LEFT..RIGHT
	...
}

note right of input
	アクションマッピング:
	button_t → uint8_t action

	例:
	input_bind(INPUT_KEY_SPACE, ACTION_JUMP);
	input_bind(INPUT_GAMEPAD_A, ACTION_JUMP);

	if (input_pressed(ACTION_JUMP)) {
		// ジャンプ処理
	}
end note

input --> button_t : uses

@enduml
```

### 7. World System (map.h/c, camera.h/c, trace.h/c)

ワールドシステムは、タイルマップ、カメラ、衝突検出を管理します。

```plantuml
@startuml
class map_t {
	+ size : vec2i_t
	+ tile_size : uint16_t
	+ name : char[16]
	+ distance : float
	+ repeat : bool
	+ foreground : bool
	+ tileset : image_t*
	+ anims : map_anim_def_t**
	+ data : uint16_t*
	+ max_tile : uint16_t
}

class camera_t {
	+ speed : float
	+ offset : vec2_t
	+ snap_to_platform : bool
	+ min_vel : float
	+ deadzone : vec2_t
	+ look_ahead : vec2_t
	+ pos : vec2_t
	+ vel : vec2_t
	+ follow : entity_ref_t
}

class trace_t {
	+ tile : int
	+ tile_pos : vec2i_t
	+ length : float
	+ pos : vec2_t
	+ normal : vec2_t
}

map_t "1" --> "*" image_t : tileset
camera_t "1" --> "0..1" entity_ref_t : follows
trace_t ..> map_t : collision on

note bottom of map_t
	JSONから読み込み:
	- Weltmeisterエディタ対応
	- パララックススクロール
	- タイルアニメーション
end note

note bottom of camera_t
	機能:
	- エンティティ追跡
	- デッドゾーン
	- ルックアヘッド
	- スムーズ移動
end note

note bottom of trace_t
	衝突検出:
	- AABB vs Tilemap
	- スイープテスト
	- 法線ベクトル計算
end note
@enduml
```

### 8. Sound System (sound.h/c)

サウンドシステムは、音源の管理と再生を担当します。

```plantuml
@startuml
class sound_source_t {
	+ samples : int16_t*
	+ len : uint32_t
	+ channels : uint32_t
	+ samplerate : uint32_t
}

class sound_t {
	+ id : uint16_t
	+ index : uint16_t
}

class "Sound System" as system {
	+ sound_source(path)
	+ sound_source_with_samples(...)
	+ sound_source_synth_sound(...)
	+ sound_source_synth_song(...)
	+ sound(source)
	+ sound_play(source)
	+ sound_play_ex(source, vol, pan, pitch)
	+ sound_unpause(sound)
	+ sound_pause(sound)
	+ sound_stop(sound)
	+ sound_set_volume(sound, volume)
	+ sound_set_pan(sound, pan)
	+ sound_set_pitch(sound, pitch)
	+ sound_set_loop(sound, loop)
}

system --> sound_source_t : manages
system --> sound_t : returns
sound_t ..> sound_source_t : references

note right of sound_source_t
	リソース:
	- QOAファイルから読み込み
	- プロシージャル生成(Synth)
	- 最大128ソース
end note

note left of sound_t
	再生ノード:
	- 最大32ノード同時再生
	- ボリューム、パン、ピッチ制御
	- ループ再生対応
end note
@enduml
```

## データフロー

### ゲームループ

```plantuml
@startuml
start

:main()起動;
:platform初期化;
:render初期化;
:sound初期化;
:input初期化;
:entities初期化;

:main_init()呼び出し;
note right
	ゲームコードが
	初期シーンを設定
end note

repeat
	:engine_update();

	partition "Update Phase" {
		:シーン切り替えチェック;
		if (新しいシーン?) then (yes)
			:旧シーンcleanup();
			:Bumpメモリリセット;
			:新シーンinit();
		endif

		:engine.tick計算;
		:scene_update()呼び出し;

		partition "Entity Update" {
			:entities_update();
			repeat
				:entity_update(ent);
				:物理シミュレーション;
				:衝突検出;
				:collide/touchコールバック;
			repeat while (全エンティティ) is (not done)
			-> done;
		}

		:camera_update();
		:engine.viewportを更新;
	}

	partition "Draw Phase" {
		:render_frame_prepare();
		:scene_draw()呼び出し;

		:背景マップ描画;
		:entities_draw();
		:前景マップ描画;

		:render_frame_end();
	}

	:input_clear();
	:temp_alloc_check();
	:Bumpをフレーム開始位置にリセット;

repeat while (ゲーム実行中?) is (yes)
-> no;

:engine_cleanup();
:platform_exit();

stop
@enduml
```

### レベル読み込み

```plantuml
@startuml
start

:engine_load_level(json_path);

:JSONファイル読み込み;
:platform_load_asset_json();

partition "マップ読み込み" {
	repeat
		:マップJSONパース;
		:map_from_json();
		:タイルセット画像読み込み;
		:バックグラウンド/コリジョン判定;
		if (collision?) then (yes)
			:engine_set_collision_map();
		else (no)
			:engine_add_background_map();
		endif
	repeat while (全マップ) is (not done)
	-> done;
}

partition "エンティティ生成" {
	repeat
		:エンティティJSON読み込み;
		:entity_type_by_name()でタイプ取得;
		:entity_spawn()でインスタンス化;
		:entity_settings()で設定適用;
	repeat while (全エンティティ) is (not done)
	-> done;
}

:JSONメモリ解放;
:temp_free();

stop
@enduml
```

## モジュール間依存関係詳細

```plantuml
@startuml
package "Foundation" {
	[types.h] as types
	[utils.h] as utils
}

package "Memory" {
	[alloc.h] as alloc
}

package "Platform" {
	[platform.h] as platform
	[input.h] as input
	[sound.h] as sound
}

package "Rendering" {
	[render.h] as render
	[image.h] as image
	[animation.h] as animation
	[font.h] as font
}

package "World" {
	[map.h] as map
	[trace.h] as trace
	[camera.h] as camera
}

package "Entity" {
	[entity_def.h] as entity_def
	[entity.h] as entity
}

package "Engine" {
	[engine.h] as engine
}

types <-- utils
types <-- alloc

types <-- platform
types <-- input
types <-- sound

types <-- render
types <-- image
image <-- animation
types <-- animation
types <-- font
font --> image

types <-- map
types <-- trace
types <-- camera
map --> image
map --> animation
trace --> map
camera --> entity_def

entity_def <-- entity
types <-- entity_def
types <-- entity
animation <-- entity
trace <-- entity

types <-- engine
map <-- engine

alloc <-- image
alloc <-- sound
alloc <-- entity
alloc <-- map

platform --> render
platform --> input
platform --> sound

engine --> entity
engine --> map
engine --> render

@enduml
```

## 設計パターンと特徴

### 1. エンティティ・コンポーネント・システム (ECS)

high_impactは軽量なECSパターンを採用していますが、厳密なECSではなく、vtableベースのポリモーフィズムを使用しています。

```plantuml
@startuml
class "entity_t (Component Data)" as entity {
	すべてのエンティティが持つ共通データ
	+ 位置、速度、サイズ
	+ 物理特性
	+ アニメーション
	+ ...
}

class "entity_vtab_t (Behavior)" as vtab {
	エンティティタイプごとの動作
	+ init(), update(), draw()
	+ collision callbacks
	+ ...
}

class "Entity System (System)" as system {
	全エンティティの管理と更新
	+ entities_update()
	+ entities_draw()
	+ 衝突検出
	+ ...
}

entity "1" --> "1" vtab : type-specific behavior
system "1" --> "*" entity : manages

note right of entity
	データ指向設計
end note

note bottom of vtab
	挙動のカプセル化
end note

note left of system
	システムロジック
end note
@enduml
```

### 2. リソース管理パターン

```plantuml
@startuml
actor "Game Code" as game

participant "Resource API\n(image/sound)" as api
participant "Resource Cache" as cache
participant "Memory Allocator" as alloc
participant "Platform" as platform

game -> api : image("path/to/image.qoi")
api -> cache : キャッシュ確認
alt キャッシュヒット
	cache --> api : 既存のimage_t*を返す
else キャッシュミス
	api -> platform : platform_load_asset()
	platform --> api : ファイルデータ(temp)
	api -> alloc : bump_alloc()
	alloc --> api : 永続メモリ
	api -> api : リソース作成
	api -> cache : キャッシュに登録
	api -> platform : temp_free()
	api --> game : 新しいimage_t*
end
@enduml
```

### 3. シーン管理とメモリライフタイム

```plantuml
@startuml
state "プログラム起動" as boot
state "シーンA" as sceneA {
	state "シーンA初期化" as initA
	state "フレームループ" as loopA
	state "フレーム処理" as frameA
	loopA : Bump位置記録
	frameA : フレームデータ割り当て
	frameA : Temp使用
	frameA : Bumpリセット
	initA --> loopA
	loopA --> frameA
	frameA --> loopA
}

state "シーンB" as sceneB {
	state "シーンB初期化" as initB
	state "フレームループ" as loopB
	loopB --> loopB
}

boot --> initA : Bump位置記録
sceneA --> initB : Bumpリセット
initB --> loopB

note right of initA
	シーンAのリソースを
	Bumpメモリに割り当て
end note

note right of initB
	シーンAのメモリを解放
	シーンBのリソースを
	Bumpメモリに割り当て
end note
@enduml
```

### 4. プラットフォーム抽象化

```plantuml
@startuml
package "Game Logic" {
	[Engine]
	[Entities]
	[Rendering]
}

package "Platform API" {
	interface "Platform Interface" as pif
	interface "Render Backend Interface" as rif
}

package "SDL Backend" {
	[platform_sdl.c]
	[render_gl.c]
}

package "Sokol Backend" {
	[platform_sokol.c]
	[render_gl.c / render_software.c]
}

Engine --> pif
Rendering --> rif

pif <|.. platform_sdl.c
pif <|.. platform_sokol.c

rif <|.. render_gl.c

note bottom of pif
	同一インターフェースで
	異なるバックエンドを切り替え
end note
@enduml
```

## ビルドとコンパイル

high_impactはライブラリではなくフレームワークとして機能します。ゲームコードと一緒にコンパイルする必要があります。

### ビルドターゲット

```plantuml
@startuml
rectangle "high_impact Source" as src {
	file "engine.c"
	file "entity.c"
	file "render.c"
	file "..."
}

rectangle "Platform Backend" as backend {
	file "platform_sdl.c" as sdl
	file "platform_sokol.c" as sokol
}

rectangle "Render Backend" as render_backend {
	file "render_gl.c" as gl
	file "render_metal.m" as metal
	file "render_software.c" as sw
}

rectangle "Game Code" as game {
	file "main.c"
	file "entities/*.c"
	file "scenes/*.c"
}

rectangle "Executable" as exe

src --> exe
backend --> exe
render_backend --> exe
game --> exe

note right of sdl
	デスクトップ向け
end note

note right of sokol
	Web/軽量向け
end note

note right of gl
	OpenGL 3.3+ / ES 3.0
end note

note right of metal
	macOS/iOS
end note

note right of sw
	ソフトウェアレンダリング
end note
@enduml
```

**Makefileターゲット:**
- `make sokol` - Sokolバックエンドでコンパイル
- `make sdl` - SDLバックエンドでコンパイル
- `make wasm` - WebAssembly向けコンパイル

## まとめ

high_impactの主な特徴:

1. **軽量設計** - 最小限の依存関係、シンプルなAPI
2. **カスタムメモリ管理** - malloc/free不使用、予測可能なメモリ使用
3. **クロスプラットフォーム** - SDL/Sokolによる複数プラットフォーム対応
4. **柔軟なレンダリング** - OpenGL/Metal/Software バックエンド
5. **シーンベース構造** - 明確なライフサイクル管理
6. **エンティティシステム** - vtableベースの拡張可能な設計
7. **統合されたツール** - Weltmeisterエディタによるレベル編集

この設計により、2Dゲーム開発に必要な機能を提供しながら、シンプルで理解しやすいコードベースを維持しています。
