#include "sound.h"
#include "utils.h"
#include "engine.h"
#include "alloc.h"
#include "platform.h"

// QOAオーディオフォーマットライブラリの実装部分を含める
// 【C言語テクニック】ヘッダオンリーライブラリの設定
#define QOA_IMPLEMENTATION  // 実装を含める
#define QOA_NO_STDIO        // 標準I/Oを使用しない
#include "../libs/qoa.h"    // Quite OK Audio フォーマット

// プロシージャル音楽シンセサイザーライブラリの実装部分を含める
#define PL_SYNTH_IMPLEMENTATION
#include "../libs/pl_synth.h"

// サウンドソースの種類を表す列挙型
// 【C言語テクニック】異なるデータ形式のサポート
typedef enum {
	SOUND_TYPE_PCM = 1,  // 生のPCMサンプルデータ
	SOUND_TYPE_QOA = 2,  // 圧縮されたQOAフォーマット
} sound_source_type_t;

// QOA形式のサウンドソース情報
// 【C言語テクニック】圧縮音声データの管理
typedef struct {
	qoa_desc desc;             // QOAファイルの説明子
	uint32_t data_len;         // 圧縮データの長さ
	uint8_t *data;             // 圧縮データ
	uint32_t pcm_buffer_start; // PCMバッファの開始サンプルインデックス
	int16_t *pcm_buffer;       // オンデマンド展開用のPCMバッファ
} sound_source_qoa_t;

// サウンドソース構造体の完全定義
// 【C言語テクニック】共用体を使用した異なるデータ形式の効率的な表現
struct sound_source_t {
	sound_source_type_t type;  // ソースのタイプ（PCMまたはQOA）
	uint32_t channels;         // チャンネル数（1=モノラル、2=ステレオ）
	uint32_t len;              // サンプル数（1チャンネルあたり）
	uint32_t samplerate;       // サンプルレート（Hz）
	union {                    // ソースデータ（排他的なデータ形式）
		int16_t *pcm_samples;    // PCM形式の場合：サンプル配列
		sound_source_qoa_t *qoa; // QOA形式の場合：QOA情報構造体
	};
};

// サウンドノード（再生インスタンス）構造体
// 【C言語テクニック】状態管理とパラメータ制御
typedef struct {
	sound_source_t *source;  // 参照しているサウンドソース
	uint16_t id;             // ノードの一意なID（有効性確認用）
	bool is_playing;         // 再生中かどうか
	bool is_halted;          // 一時停止中かどうか
	bool is_looping;         // ループ再生するかどうか
	float pan;               // パン位置（-1=左、0=中央、1=右）
	float volume;            // ボリューム（0〜1、最大16まで）
	float pitch;             // ピッチ（再生速度、デフォルト1）
	float sample_pos;        // 現在のサンプル位置
} sound_node_t;


// サウンドシステム ----------------------------------------------------------------
// 【C言語テクニック】グローバル状態の効率的な管理

// グローバルボリューム（0〜1）
static float global_volume = 1;

// 出力サンプルレートの逆数（最適化用）
static float inv_out_samplerate;

// サウンドソースの配列とその管理情報
// 【C言語テクニック】固定サイズ配列によるリソース管理
static sound_source_t sources[SOUND_MAX_SOURCES];          // ソース配列
static uint32_t sources_len = 0;                           // 現在のソース数
static char *source_paths[SOUND_MAX_SOURCES] = {};         // ソースのパス配列（キャッシュ用）

// サウンドノードの配列と管理情報
static sound_node_t sound_nodes[SOUND_MAX_NODES];          // ノード配列
static uint32_t nodes_len = 0;                             // 現在のノード数（未使用）
static uint16_t sound_unique_id = 0;                       // ノードIDカウンタ
static bool sound_synth_initialized = false;               // シンセサイザー初期化済みフラグ


// サウンドシステムの初期化
// 【C言語テクニック】逆数を事前計算して最適化
void sound_init(int samplerate) {
	// サンプルレートの逆数を計算（後の計算を高速化するため）
	inv_out_samplerate = 1.0 / samplerate;
}

// シンセサイザーの初期化
// 【C言語テクニック】遅延初期化パターン
void sound_init_synth(void) {
	// 既に初期化されている場合は何もしない
	if (sound_synth_initialized) {
		return;
	}
	// シンセサイザーメモリを割り当てて初期化
	pl_synth_init(bump_alloc(PL_SYNTH_TAB_SIZE));
	sound_synth_initialized = true;
}

// サウンドシステムのクリーンアップ
void sound_cleanup(void) {
	// すべてのノードを停止
	for (int i = 0; i < SOUND_MAX_NODES; i++) {
		sound_nodes[i].is_playing = false;
	}
}

// 現在のサウンドリソース状態をマーク
// 【C言語テクニック】リソース状態のスナップショット
sound_mark_t sound_mark(void) {
	return (sound_mark_t){.index = sources_len};
}

// 指定されたマークまでサウンドリソースをリセット
// 【C言語テクニック】シーン切り替え時のリソース管理
void sound_reset(sound_mark_t mark) {
	// 無効化されるソースを参照しているノードをリセット
	for (int i = 0; i < SOUND_MAX_NODES; i++) {
		sound_node_t *node = &sound_nodes[i];
		// ノードのソースがリセット対象範囲内にある場合
		if (node->source - sources >= mark.index) {
			node->id = 0;           // ノードを無効化
			node->is_playing = false; // 再生停止
			node->is_halted = false;  // 一時停止状態もクリア
			node->is_looping = false; // ループ設定もクリア
		}
	}
	// ソース数を指定されたマーク位置に戻す
	sources_len = mark.index;
}

// すべての再生中ノードを一時停止状態にする
// 【C言語テクニック】再生状態のグローバル管理
void sound_halt(void) {
	for (int i = 0; i < SOUND_MAX_NODES; i++) {
		if (sound_nodes[i].is_playing) {
			sound_nodes[i].is_playing = false; // 再生を停止
			sound_nodes[i].is_halted = true;   // 一時停止状態をマーク
		}
	}
}

// 一時停止中のすべてのサウンドを再開
void sound_resume(void) {
	for (int i = 0; i < SOUND_MAX_NODES; i++) {
		if (sound_nodes[i].is_halted) {
			sound_nodes[i].is_playing = true;  // 再生を再開
			sound_nodes[i].is_halted = false;  // 一時停止状態を解除
		}
	}
}

// グローバルボリュームを取得
float sound_global_volume(void) {
	return global_volume;
}

// グローバルボリュームを設定
// 【C言語テクニック】値の範囲制限
void sound_set_global_volume(float volume) {
	// ボリュームを0〜1の範囲に制限
	global_volume = clamp(volume, 0, 1);
}

// サウンドミキシング処理（ステレオ出力用）
// 【C言語テクニック】オーディオバッファへの直接ミキシング
void sound_mix_stereo(float *dest_samples, uint32_t dest_len) {
	// 出力バッファを初期化（0埋め）
	memset(dest_samples, 0, dest_len * sizeof(float));

	// サンプルはint16_t形式で格納されているため、
	// グローバルボリュームと一緒に正規化（int16_tからfloat（-1..1））も行う
	// 【C言語テクニック】変換と処理を同時に行う最適化
	float volume_normalize = global_volume / 32768.0;  // int16_tの最大値で割る

	// すべてのサウンドノードをループ処理
	for (uint32_t n = 0; n < SOUND_MAX_NODES; n++) {
		sound_node_t *node = &sound_nodes[n];

		// 再生中で、かつボリュームが0より大きいノードのみ処理
		if (node->is_playing && node->volume > 0) {
			sound_source_t *source = node->source;
			
			// パンポジションに基づいた左右チャンネルのボリューム計算
			// 【C言語テクニック】ステレオパンの簡易実装
			float vol_left = volume_normalize * node->volume * clamp(1.0 - node->pan, 0, 1);
			float vol_right = volume_normalize * node->volume * clamp(1.0 + node->pan, 0, 1);

			// 出力サンプルレートを考慮してピッチを計算
			// 注意: クオリティ的には、これはソースを「リサンプル」する最良の方法ではない
			// TODO: より高品質なリサンプリング実装が必要
			float pitch = node->pitch * source->samplerate * inv_out_samplerate;

			// ソースデータへのポインタを取得
			sound_source_qoa_t *qoa = NULL;
			int16_t *src_samples = NULL;

			// ソースタイプに応じてサンプルポインタを設定
			if (source->type == SOUND_TYPE_PCM) {
				src_samples = source->pcm_samples;  // PCM形式の場合
			}
			else if (source->type == SOUND_TYPE_QOA) {
				qoa = source->qoa;                  // QOA形式の場合
				src_samples = source->qoa->pcm_buffer;
			}

			// チャンネル数フラグ（モノラル=0、ステレオ=1）
			// 【C言語テクニック】ビットシフトによるインデックス計算の最適化
			int c = source->channels == 2 ? 1 : 0;

			// 出力バッファの各サンプルを処理（ステレオなので2つずつ）
			for (uint32_t di = 0; di < dest_len; di += 2) {
				// 整数のサンプル位置を取得
				uint32_t source_index = (uint32_t)node->sample_pos;

				// 圧縮されたソースの場合、このサンプル位置に対応するフレームを
				// デコードする必要があるかチェック
				// 【C言語テクニック】オンデマンドデコンプレッション
				if (source->type == SOUND_TYPE_QOA) {
					// サンプル位置が現在バッファにデコードされているフレームの範囲外
					if (
						source_index < qoa->pcm_buffer_start || 
						source_index >= qoa->pcm_buffer_start + QOA_FRAME_LEN
					) {
						// 新しいフレームをデコード
						uint32_t frame_index = source_index / QOA_FRAME_LEN;
						uint32_t frame_data_start = qoa_max_frame_size(&qoa->desc) * frame_index;
						uint32_t frame_data_len = qoa->data_len - frame_data_start;
						void *frame_data = qoa->data + frame_data_start;
						uint32_t frame_len;
						
						// フレームをデコードしてPCMバッファに格納
						qoa_decode_frame(frame_data, frame_data_len, &qoa->desc, src_samples, &frame_len);
						qoa->pcm_buffer_start = frame_index * QOA_FRAME_LEN;
					}
					// バッファ内でのオフセットを計算
					source_index -= qoa->pcm_buffer_start;
				}
				
				// サンプルを出力バッファにミックス（左右チャンネル）
				// 【C言語テクニック】サンプルのミキシングと加重
				dest_samples[di+0] += src_samples[(source_index << c) + 0] * vol_left;
				dest_samples[di+1] += src_samples[(source_index << c) + c] * vol_right;

				// サンプル位置を進める（ピッチに応じて）
				node->sample_pos += pitch;
				
				// 終端チェック
				if (node->sample_pos >= source->len || node->sample_pos < 0) {
					if (node->is_looping) {
						// ループ設定の場合、位置を折り返す
						// 【C言語テクニック】負の値をループさせる正しい方法
						node->sample_pos = 
							fmod(node->sample_pos, source->len) + 
							(node->sample_pos < 0 ? source->len : 0);
					}
					else {
						// ループしない場合は再生終了
						node->is_playing = false;
						break;
					}
				}
			}
		}
	}
}


// サウンドソース機能 ------------------------------------------------------------------

// QOAファイルからサウンドソースを読み込む
// 【C言語テクニック】リソースのキャッシングとオンデマンド展開
sound_source_t *sound_source(char *path) {
	// キャッシュ検索：既に読み込まれている場合はそのまま返す
	for (uint32_t i = 0; i < sources_len; i++) {
		if (str_equals(path, source_paths[i])) {
			return &sources[i];
		}
	}

	// 制限チェック
	error_if(sources_len >= SOUND_MAX_SOURCES, "Max sound sources (%d) reached", SOUND_MAX_SOURCES);
	error_if(engine_is_running(), "Cant load sound source during gameplay");

	// アセットファイルの読み込み
	uint32_t file_size;
	uint8_t *data = platform_load_asset(path, &file_size);
	error_if(data == NULL, "Failed to load sound %s", path);

	// QOAヘッダーのデコード
	qoa_desc desc;
	uint32_t read_pos = qoa_decode_header(data, file_size, &desc);
	error_if(read_pos == 0, "Failed to decode sound %s", path);
	error_if(desc.channels > 2, "QOA file %s has more than 2 channels", path);

	// 新しいサウンドソースを初期化
	sound_source_t *source = &sources[sources_len];
	source->channels = desc.channels;
	source->len = desc.samples;
	source->samplerate = desc.samplerate;

	// 総サンプル数を計算
	uint32_t total_samples = desc.samples * desc.channels;

	// ソースが十分に短い場合は、読み込み時に完全に展開
	// 【C言語テクニック】メモリ使用量と処理速度のトレードオフ
	if (total_samples <= SOUND_MAX_UNCOMPRESSED_SAMPLES) {
		source->type = SOUND_TYPE_PCM;
		source->pcm_samples = bump_alloc(total_samples * sizeof(int16_t));

		// 全フレームを順次デコード
		uint32_t sample_index = 0;
		uint32_t frame_len;
		uint32_t frame_size;

		do {
			int16_t *sample_ptr = source->pcm_samples + sample_index * desc.channels;
			frame_size = qoa_decode_frame(data + read_pos, file_size - read_pos, &desc, sample_ptr, &frame_len);
			error_if(frame_size == 0, "QOA decode error for file %s", path);

			read_pos += frame_size;
			sample_index += frame_len;
		} while (frame_size && sample_index < desc.samples);

		// 一時メモリを解放
		temp_free(data);
	}

	// 長いソースはオンデマンドで展開するため、圧縮されたままメモリに保持
	// 最初のフレームだけをここでデコードしておく
	else {
		uint32_t qoa_data_size = file_size - read_pos;

		// データを一時メモリから永続メモリに転送
		uint8_t *bump_data = bump_from_temp(data, read_pos, qoa_data_size);

		// QOA情報構造体を初期化
		source->type = SOUND_TYPE_QOA;
		source->qoa = bump_alloc(sizeof(sound_source_qoa_t));
		source->qoa->desc = desc;
		source->qoa->data = bump_data;
		source->qoa->data_len = qoa_data_size;
		source->qoa->pcm_buffer_start = 0;
		source->qoa->pcm_buffer = bump_alloc(desc.channels * QOA_FRAME_LEN * sizeof(int16_t));

		// 最初のフレームをデコード
		uint32_t frame_len;
		uint32_t frame_size = qoa_decode_frame(
			source->qoa->data, file_size - read_pos, 
			&desc, source->qoa->pcm_buffer, &frame_len
		);
		error_if(frame_size == 0, "QOA decode error for file %s", path);
	}
	
	// パス文字列を永続メモリに保存（キャッシュ用）
	source_paths[sources_len] = bump_alloc(strlen(path)+1);
	strcpy(source_paths[sources_len], path);

	// ソース数を増やして返す
	sources_len++;
	return source;
}

// 内部生成音源のパス識別子
static char *sound_internal_path = "__internal";

// 生のサンプルからサウンドソースを作成
// 【C言語テクニック】既存のサンプルデータの再利用
sound_source_t *sound_source_with_samples(int16_t *samples, uint32_t len, uint32_t channels, uint32_t samplerate) {
	// 制限チェック
	error_if(sources_len >= SOUND_MAX_SOURCES, "Max sound sources (%d) reached", SOUND_MAX_SOURCES);
	error_if(engine_is_running(), "Cant load sound source during gameplay");

	// 新しいソースを初期化
	sound_source_t *source = &sources[sources_len];
	source->channels = channels;
	source->len = len;
	source->samplerate = samplerate;
	source->type = SOUND_TYPE_PCM;
	source->pcm_samples = samples;  // 所有権は取得しない（コピーもしない）

	// 内部生成音源としてマーク
	source_paths[sources_len] = sound_internal_path;

	// ソース数を増やして返す
	sources_len++;
	return source;
}

// シンセサイザーでサウンドエフェクトを生成
// 【C言語テクニック】プロシージャル生成サウンド
sound_source_t *sound_source_synth_sound(pl_synth_sound_t *sound) {
	// シンセサイザーが初期化されていることを確認
	sound_init_synth();
	
	// サウンドの長さを取得
	int len = pl_synth_sound_len(sound);
	
	// サンプルバッファを割り当て（ステレオなので*2）
	int16_t *samples = bump_alloc(sizeof(int16_t) * len * 2);
	
	// シンセサイザーでサウンドを生成
	pl_synth_sound(sound, samples);
	
	// 生成したサンプルでソースを作成
	return sound_source_with_samples(samples, len, 2, PL_SYNTH_SAMPLERATE);
}

// シンセサイザーで音楽トラックを生成
// 【C言語テクニック】一時メモリと永続メモリの使い分け
sound_source_t *sound_source_synth_song(pl_synth_song_t *song) {
	// シンセサイザーが初期化されていることを確認
	sound_init_synth();
	
	// 曲の長さを取得
	int len = pl_synth_song_len(song);
	
	// 永続サンプルバッファを割り当て
	int16_t *samples = bump_alloc(sizeof(int16_t) * len * 2);
	
	// 一時的な作業バッファを割り当て
	int16_t *temp_samples = temp_alloc(sizeof(int16_t) * len * 2);
	
	// シンセサイザーで音楽を生成
	pl_synth_song(song, samples, temp_samples);
	
	// 一時バッファを解放
	temp_free(temp_samples);
	
	// 生成したサンプルでソースを作成
	return sound_source_with_samples(samples, len, 2, PL_SYNTH_SAMPLERATE);
}

// サウンドソースの再生時間（秒）を計算
float sound_source_duration(sound_source_t *source) {
	// サンプル数 ÷ サンプルレート = 秒数
	return source->len / source->samplerate;
}


// サウンドノード機能 ------------------------------------------------------------------

// 空きノードを取得し、指定されたソースで初期化
// 【C言語テクニック】オブジェクトプールからのリソース割り当て
sound_t sound(sound_source_t *source) {
	// 無効なサウンドハンドルを初期化
	sound_t sound = {.id = 0, .index = 0};
	sound_node_t *node = NULL;

	// 現在再生中ではないノードを探す（最優先）
	for (int i = 0; i < SOUND_MAX_NODES; i++) {
		if (!sound_nodes[i].is_playing && !sound_nodes[i].is_halted && !sound_nodes[i].id){
			node = &sound_nodes[i];
			sound.index = i;
			break;
		}
	}

	// 予約されていないノードにフォールバック
	// これにより予約されていない再生中のノードが中断される
	if (!node) {
		for (int i = 0; i < SOUND_MAX_NODES; i++) {
			if (!sound_nodes[i].id) {
				node = &sound_nodes[i];
				sound.index = i;
				break;
			}
		}
	}

	// それでも見つからない場合は無効なハンドルを返す
	if (!node) {
		return sound;
	}

	// 一意なIDを生成（ラップアラウンドを考慮）
	sound_unique_id++;
	if (sound_unique_id == 0) {
		sound_unique_id = 1;  // 0は無効なIDとして予約
	}

	// ノードを初期化
	node->id = sound_unique_id;
	node->is_playing = false;    // 一時停止状態で開始
	node->is_halted = false;
	node->is_looping = false;
	node->source = source;
	node->volume = 1;            // デフォルトボリューム
	node->pan = 0;               // デフォルトは中央
	node->sample_pos = 0;        // 先頭から開始
	node->pitch = 1;             // デフォルトピッチ

	// サウンドハンドルを完成させて返す
	sound.id = sound_unique_id;
	return sound;
}

// サウンドソースをワンショット再生
// 【C言語テクニック】自動リソース管理
void sound_play(sound_source_t *source) {
	// ノードを取得して再生開始し、自動的に破棄設定
	sound_t s = sound(source);
	sound_unpause(s);
	sound_dispose(s);
}

// 拡張パラメータ付きでサウンドソースをワンショット再生
// 【C言語テクニック】便利なラッパー関数
void sound_play_ex(sound_source_t *source, float volume, float pan, float pitch) {
	// ノードを取得
	sound_t s = sound(source);
	
	// 各種パラメータを設定
	sound_set_volume(s, volume);
	sound_set_pan(s, pan);
	sound_set_pitch(s, pitch);
	
	// 再生開始し、自動的に破棄設定
	sound_unpause(s);
	sound_dispose(s);
}


// サウンドハンドルから実際のノードを取得するヘルパー関数
// 【C言語テクニック】IDによる安全な参照解決
static inline sound_node_t *sound_get_node(sound_t sound) {
	// インデックスの範囲とIDが一致するか確認
	if (sound.index < SOUND_MAX_NODES && sound_nodes[sound.index].id == sound.id) {
		return &sound_nodes[sound.index];
	}
	else {
		return NULL;  // 無効なハンドルまたは削除されたノード
	}
}

// 一時停止中のノードを再開
void sound_unpause(sound_t sound) {
	sound_node_t *node = sound_get_node(sound);
	if (!node) {
		return;
	}

	// 再生フラグを設定
	node->is_playing = true;
	node->is_halted = false;
}

// ノードを一時停止
void sound_pause(sound_t sound) {
	sound_node_t *node = sound_get_node(sound);
	if (!node) {
		return;
	}

	// 再生フラグをクリア
	node->is_playing = false;
	node->is_halted = false;
}

// ノードを停止して先頭に巻き戻し
void sound_stop(sound_t sound) {
	sound_node_t *node = sound_get_node(sound);
	if (!node) {
		return;
	}

	// 位置を先頭に戻し、再生フラグをクリア
	node->sample_pos = 0;
	node->is_playing = false;
	node->is_halted = false;
}

// ノードを破棄（予約解除）
// 【C言語テクニック】リソース解放と再生の分離
void sound_dispose(sound_t sound) {
	sound_node_t *node = sound_get_node(sound);
	if (!node) {
		return;
	}

	// ループをオフにしてIDをクリア（予約解除）
	// 注意：再生中のノードはそのまま再生を続ける
	node->is_looping = false;
	node->id = 0;
}

// ノードのループ状態を取得
bool sound_loop(sound_t sound) {
	sound_node_t *node = sound_get_node(sound);
	if (!node) {
		return false;
	}

	return node->is_looping;
}

// ノードのループ状態を設定
void sound_set_loop(sound_t sound, bool loop) {
	sound_node_t *node = sound_get_node(sound);
	if (!node) {
		return;
	}

	node->is_looping = loop;
}

// ノードの総再生時間（秒）を取得
float sound_duration(sound_t sound) {
	sound_node_t *node = sound_get_node(sound);
	if (!node) {
		return 0;
	}

	// 基となるソースの再生時間を返す
	return sound_source_duration(node->source);
}

// ノードの現在の再生位置（秒）を取得
float sound_time(sound_t sound) {
	sound_node_t *node = sound_get_node(sound);
	if (!node) {
		return 0;
	}

	// サンプル位置をサンプルレートで割って秒に変換
	return node->sample_pos / node->source->samplerate;
}

// ノードの再生位置（秒）を設定
void sound_set_time(sound_t sound, float time) {
	sound_node_t *node = sound_get_node(sound);
	if (!node) {
		return;
	}

	// 秒をサンプル位置に変換し、有効な範囲に制限
	node->sample_pos = clamp(time / node->source->samplerate, 0, node->source->len);
}

// ノードのボリュームを取得
float sound_volume(sound_t sound) {
	sound_node_t *node = sound_get_node(sound);
	if (!node) {
		return 0;
	}

	return node->volume;
}

// ノードのボリュームを設定
void sound_set_volume(sound_t sound, float volume) {
	sound_node_t *node = sound_get_node(sound);
	if (!node) {
		return;
	}

	// ボリュームを0〜16の範囲に制限
	node->volume = clamp(volume, 0, 16);
}

// ノードのパン位置を取得
float sound_pan(sound_t sound) {
	sound_node_t *node = sound_get_node(sound);
	if (!node) {
		return 0;
	}

	return node->pan;
}

// ノードのパン位置を設定
void sound_set_pan(sound_t sound, float pan) {
	sound_node_t *node = sound_get_node(sound);
	if (!node) {
		return;
	}

	// パンを-1〜1の範囲に制限（-1=左、0=中央、1=右）
	node->pan = clamp(pan, -1, 1);
}

// ノードのピッチを取得
float sound_pitch(sound_t sound) {
	sound_node_t *node = sound_get_node(sound);
	if (!node) {
		return 0;
	}

	return node->pitch;
}

// ノードのピッチを設定
void sound_set_pitch(sound_t sound, float pitch) {
	sound_node_t *node = sound_get_node(sound);
	if (!node) {
		return;
	}

	// ピッチを設定（制限なし、負の値は逆再生）
	node->pitch = pitch;
}
