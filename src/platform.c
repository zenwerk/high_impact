#include "platform.h"

// プラットフォーム実装の選択
// -----------------------------------------------------------------------------
// コンパイル時にPLATFORM_SDLまたはPLATFORM_SOKOLが定義されているかに応じて、
// 対応するプラットフォーム実装ファイルを含めます。
//
// 【初心者向け解説】
// プリプロセッサ条件分岐（#if, #elif, #endif）を使用して、コンパイル時に
// どのプラットフォーム実装を使用するかを選択しています。
//
// - SDL実装: より多機能で成熟したマルチメディアライブラリを使用
// - Sokol実装: より軽量でシンプルな実装（Webブラウザ対応に優れる）
//
// これにより、同じゲームコードを異なるプラットフォームライブラリ上で
// 実行できるようになります。開発者はコンパイル時にどちらかを選ぶだけで、
// ゲームコード自体を変更する必要はありません。
#if defined(PLATFORM_SDL)
	#include "platform_sdl.c"
#elif defined(PLATFORM_SOKOL)
	#include "platform_sokol.c"
#else
	#error "No platform specified. #define PLATFORM_SDL or PLATFORM_SOKOL"
#endif

// OSごとの依存関係
// -----------------------------------------------------------------------------
// 実行可能ファイルのパスを取得するために必要なOSごとのヘッダーファイルを含めます。
//
// 【初心者向け解説】
// 異なるオペレーティングシステム（Windows、macOS、Linux）では、
// システム情報を取得するための異なるAPIが必要です。このコードは
// コンパイル時に適切なヘッダーを選択します。
#if defined(_WIN32)
	#include <windows.h>
#elif defined(__APPLE__)
	#include <mach-o/dyld.h>
#elif defined(__linux__)
	#include <unistd.h>
#endif

// JSONアセットの読み込み
// -----------------------------------------------------------------------------
// アセットデータとしてJSONファイルを読み込み、解析します。
//
// 【初心者向け解説】
// JSONは「JavaScript Object Notation」の略で、データを構造化して保存するための
// テキスト形式です。ゲーム開発では、レベル設計、キャラクター設定、ゲーム設定などの
// データを保存するためによく使われます。
//
// この関数は、まず生のアセットデータを読み込み、次にそれをJSON形式として解析します。
// 一時メモリ（temp_free）を使用して、データの解析後にメモリを解放しています。
json_t *platform_load_asset_json(const char *name) {
	uint32_t len;
	uint8_t *data = platform_load_asset(name, &len);
	if (data == NULL) {
		return NULL;
	}
	json_t *v = json_parse(data, len);
	temp_free(data);
	return v;
}

// 実行可能ファイルのパスを取得
// -----------------------------------------------------------------------------
// 現在実行中のプログラムの実行可能ファイルへの完全なパスを取得します。
//
// 【初心者向け解説】
// この関数は「ゲームがどこにインストールされているか」を知るために使用されます。
// これは、ゲームがアセットファイル（画像、音声、マップデータなど）を見つけるために
// 重要です。
//
// 異なるOSでは、実行可能ファイルのパスを取得する方法が異なります：
// - Windows: GetModuleFileName関数を使用
// - macOS: _NSGetExecutablePath関数を使用
// - Linux: /proc/self/exeというシンボリックリンクを読み取り
//
// この違いを吸収することで、同じコードをどのOSでも動作させることができます。
char *platform_executable_path(void) {
	uint32_t buffer_len = 2048;
	char buffer[buffer_len];

	#if defined(_WIN32)
		if (GetModuleFileName(NULL, buffer, (DWORD)buffer_len) == 0) {
			return NULL;
		}
	#elif defined(__APPLE__)
		if (_NSGetExecutablePath(buffer, &buffer_len) != 0) {
			return NULL;
		}
	#elif defined(__linux__)
		ssize_t len = readlink("/proc/self/exe", buffer, buffer_len - 1);
		if (len < 0) {
			return NULL;
		}
		buffer[len] = '\0';
	#else
		return NULL;
	#endif

	return str_format("%s", buffer);
}

// ファイルパスからディレクトリ名を取得
// -----------------------------------------------------------------------------
// 完全なファイルパスからディレクトリ部分だけを抽出します。
//
// 【初心者向け解説】
// この関数は、パス（例："/home/user/game/assets/image.png"）から
// ディレクトリ部分（"/home/user/game/assets/"）を取得します。
//
// パスの区切り文字はOSによって異なります：
// - Windowsではバックスラッシュとフォワードスラッシュの両方がサポートされています
//   （例：「C:\games\my_game\」または「C:/games/my_game/」）
// - macOSとLinuxではフォワードスラッシュのみ（例：「/Applications/game/」）
//
// この関数は、アセットが実行可能ファイルと同じディレクトリにあるかの確認や、
// ファイルの読み込みなどに使用されます。
char *platform_dirname(char *path) {
	#if defined(_WIN32)
		// Windowsでは、最後のバックスラッシュまたはフォワードスラッシュを探す
		char *last_slash = max(strrchr(path, '/'), strrchr(path, '\\'));
	#else
		// macOSとLinuxでは、最後のフォワードスラッシュを探す
		char *last_slash = strrchr(path, '/');
	#endif
	if (last_slash == NULL) {
		// スラッシュが見つからない場合、空の文字列を返す（現在のディレクトリ）
		return str_format("");
	}
	// パスの先頭からスラッシュまでの部分を返す（スラッシュを含む）
	return str_format("%.*s", last_slash - path + 1, path);
}
