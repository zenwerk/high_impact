#ifndef HI_SOUND_H
#define HI_SOUND_H

// サウンドシステム
// サウンドは2つの部分に分かれています：サウンドソース（sound_source_t）と
// 現在再生中のサウンドを表す「ノード」（sound_t）です。
// 各ノードはソースのいずれかを使用します。
// 【C言語テクニック】リソースと再生インスタンスの分離

#include "types.h"
#include "../libs/pl_synth.h"

// サウンドソースが完全に展開されるサンプル数の上限
// この上限を超えるサンプルは圧縮された形式でメモリに保持され、
// 必要に応じて展開されます。
// 【C言語テクニック】メモリ使用効率の最適化
#if !defined(SOUND_MAX_UNCOMPRESSED_SAMPLES)
	#define SOUND_MAX_UNCOMPRESSED_SAMPLES (64 * 1024)
#endif

// 同時に読み込めるサウンドソースの最大数
// これはメモリ使用量に影響しますが、パフォーマンスには影響しません。
#if !defined(SOUND_MAX_SOURCES)
	#define SOUND_MAX_SOURCES 128
#endif

// 同時にミックスできるアクティブノードの最大数
// 【C言語テクニック】リソース制限によるパフォーマンス確保
#if !defined(SOUND_MAX_NODES)
	#define SOUND_MAX_NODES 32
#endif


// サウンドソース型（前方宣言）
typedef struct sound_source_t sound_source_t;

// サウンドノード型（ハンドル）
// 【C言語テクニック】ハンドルによるリソース参照
typedef struct { uint16_t id; uint16_t index; } sound_t;

// サウンドマーク型（リソース管理用）
typedef struct { uint32_t index; } sound_mark_t;

// sound_source_from_synth_*()関数用にシンセサイザーを初期化
void sound_init_synth(void);

// エンジンによってサウンドメモリを管理するための関数
// 【C言語テクニック】リソース状態のマーキングとリセット
sound_mark_t sound_mark(void);
void sound_reset(sound_mark_t mark);

// すべての再生中ノードを一時停止状態にする（ポーズ画面などに便利）
void sound_halt(void);

// 一時停止中のすべてのサウンドを再開する
void sound_resume(void);

// すべてのサウンドのグローバルボリュームを取得
float sound_global_volume(void);

// すべてのノードのグローバルボリュームを設定
void sound_set_global_volume(float volume);

// プラットフォームから定期的に呼び出され、再生中のノードを出力バッファにミックス
// 【C言語テクニック】オーディオバッファへのミキシング
void sound_mix_stereo(float *dest_samples, uint32_t dest_len);

// QOAファイルからサウンドソースを読み込む
// 同じパスで複数回呼び出すと、同じキャッシュされたサウンドソースを返す
// 【C言語テクニック】リソースキャッシング
sound_source_t *sound_source(char *path);

// 生のサンプルからサウンドソースを初期化
// サンプルの所有権は取得されず、コピーもされない
sound_source_t *sound_source_with_samples(int16_t *samples, uint32_t len, uint32_t channels, uint32_t samplerate);

// 指定されたpl_synth_sound_t定義でサウンドソースを作成
// 【C言語テクニック】プロシージャル生成サウンド
sound_source_t *sound_source_synth_sound(pl_synth_sound_t *sound);

// 指定されたpl_synth_song_t定義でサウンドソースを作成
sound_source_t *sound_source_synth_song(pl_synth_song_t *song);

// サウンドソースの再生時間（秒）を返す
float sound_source_duration(sound_source_t *source);

// 指定されたソースの空きノードを取得する
// これによりソースが「予約」され、sound_dispose()で解放されるまで再利用できなくなる
// ノードは一時停止状態になり、明示的に再開する必要がある
// 空きノードがない場合はid = 0の無効なノードを返す
// 【C言語テクニック】オブジェクトプールとリソース予約
sound_t sound(sound_source_t *source);

// サウンドソースを再生する
// 再生に使用されるノードは再生終了後に自動的に破棄される
// 【C言語テクニック】簡易インターフェース
void sound_play(sound_source_t *source);

// 指定されたボリューム、パン、ピッチでサウンドソースを再生する
// 再生に使用されるノードは再生終了後に自動的に破棄される
// 【C言語テクニック】拡張パラメータによる柔軟性
void sound_play_ex(sound_source_t *source, float volume, float pan, float pitch);

// 一時停止中のノードを再開する
void sound_unpause(sound_t sound);

// ノードを一時停止する
void sound_pause(sound_t sound);

// ノードを一時停止して先頭に巻き戻す
void sound_stop(sound_t sound);

// このノードを破棄する
// 破棄後、ノードは無効になるが、一時停止されていなければ最後まで再生される
// 【C言語テクニック】リソース解放と自動再生管理
void sound_dispose(sound_t sound);

// このノードがループするかどうかを返す
bool sound_loop(sound_t sound);

// このノードをループするかどうかを設定する
void sound_set_loop(sound_t sound, bool loop);

// 基となるサウンドソースの再生時間（秒）を返す
// ノードの現在のピッチは考慮されない
float sound_duration(sound_t sound);

// ノードの現在の位置（秒）を返す
// ノードの現在のピッチは考慮されない
float sound_time(sound_t sound);

// ノードの現在の位置（秒）を設定する
// ノードの現在のピッチは考慮されない
void sound_set_time(sound_t sound, float time);

// ノードの現在のボリュームを返す
float sound_volume(sound_t sound);

// ノードの現在のボリュームを設定する
void sound_set_volume(sound_t sound, float volume);

// ノードの現在のパン位置を返す（-1 = 左, 0 = 中央, 1 = 右）
float sound_pan(sound_t sound);

// ノードの現在のパン位置を設定する
void sound_set_pan(sound_t sound, float pan);

// ノードの現在のピッチ（再生速度）を返す。デフォルトは1
float sound_pitch(sound_t sound);

// ノードの現在のピッチ（再生速度）を設定する
void sound_set_pitch(sound_t sound, float pitch);

// プラットフォームによって呼び出される初期化・クリーンアップ関数
// 【C言語テクニック】プラットフォーム層とのインターフェース
void sound_init(int samplerate);
void sound_cleanup(void);

#endif
