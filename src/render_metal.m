#include "engine.h"
#include "render.h"
#include "alloc.h"
#include "utils.h"
#include "platform.h"

// Metalレンダラー実装
// =============================================================================
// 【初心者向け解説】
// このファイルは、Apple製品（macOSやiOS）向けのMetalグラフィックスAPIを使用した
// レンダラーの実装です。Metalはアップルが開発した高性能なグラフィックスAPIで、
// OpenGLよりも効率的で低レベルなハードウェアアクセスを提供します。
//
// 他のレンダラー実装と比較：
// - OpenGL: 広く使われているクロスプラットフォームなAPI
// - Metal: Apple製品専用だが高速で効率的
// - ソフトウェア: 最も互換性があるが性能は低い
//
// このファイルはObjective-Cで書かれています（.m拡張子）。これはAppleの
// フレームワークとの互換性のためです。

// バッファ容量設定
// -----------------------------------------------------------------------------
#if !defined(RENDER_BUFFER_CAPACITY)
	#define RENDER_BUFFER_CAPACITY 2048  // 一度に描画できる四角形の最大数
#endif

// ミップマップ設定
// -----------------------------------------------------------------------------
#if !defined(RENDER_USE_MIPMAPS)
	#define RENDER_USE_MIPMAPS 0  // テクスチャの縮小表示品質向上機能
#endif

// 必要な環境チェック
// -----------------------------------------------------------------------------
// Objective-Cモードでコンパイルする必要がある
#if !defined(__OBJC__)
	#error Metal renderer must be compiled in Objective-C mode (-x objective-c).
#endif

// 自動参照カウント（ARC）が必要
#if !__has_feature(objc_arc)
	#error Metal renderer requires Objective-C ARC (-fobjc-arc).
#endif

// Appleプラットフォーム専用
#if defined(__APPLE__)
	// 必要なフレームワークをインポート
	#import <CoreGraphics/CoreGraphics.h>  // グラフィックス基本機能
	#import <Metal/Metal.h>                // Metal API
	#import <QuartzCore/CAMetalLayer.h>    // Metal描画レイヤー
	#include <simd/simd.h>                 // ベクトル/行列演算用
#else
	#error Compiling Metal renderer for non-Apple platform is not supported.
#endif

// シェーダー定義
// =============================================================================
// 【初心者向け解説】
// シェーダーとは、GPUで実行される小さなプログラムです。
// Metalでは、これらのプログラムはObjective-CまたはSwiftから呼び出され、
// グラフィックスハードウェアで実行されて、実際の描画を行います。

// インスタンスアライメント設定
// -----------------------------------------------------------------------------
// 【初心者向け解説】
// フレームの途中でバッファをフラッシュする（描画を実行する）際に、
// メモリアライメント（メモリ上での配置）が重要になります。
// 
// Metalデバイスには最小バッファオフセットアライメント要件があり、
// 四角形の頂点データを効率的に処理するために、この要件に合わせる必要があります。
// Mac向けGPUでは、この最小アライメントは32バイトです。
#define RENDER_INSTANCE_ALIGNMENT 32

static char *const shaderSource = ""
"#include <metal_stdlib>\n"
"using namespace metal;\n"
"\n"
"#if !defined(RENDER_TEXTURES_MAX)\n"
"	#define RENDER_TEXTURES_MAX 1024\n"
"#endif\n"
"\n"
"#if !defined(RENDER_INSTANCE_ALIGNMENT)\n"
"	#define RENDER_INSTANCE_ALIGNMENT 32\n"
"#endif\n"
"\n"
"struct __attribute__((aligned(RENDER_INSTANCE_ALIGNMENT))) QuadInstance {\n"
"	float2 positions[4];\n"
"	float2 uvs[4];\n"
"	uint colors[4];\n"
"	uint textureIndex;\n"
"};\n"
"\n"
"struct VertexOut {\n"
"	float4 pos [[position]];\n"
"	float2 uv;\n"
"	float4 color;\n"
"	uint textureIndex [[flat]];\n"
"};\n"
"\n"
"struct Uniforms {\n"
"	float2 screen;\n"
"	float2 fade;\n"
"	float time;\n"
"};\n"
"\n"
"struct Arguments {\n"
"	texture2d<float> textures[RENDER_TEXTURES_MAX];\n"
"	sampler textureSampler;\n"
"};\n"
"\n"
"vertex VertexOut vertex_main(device const QuadInstance *instances [[buffer(0)]],\n"
"							 constant Uniforms &u                  [[buffer(1)]],\n"
"							 uint instanceID                       [[instance_id]],\n"
"							 uint vertexID                         [[vertex_id]])\n"
"{\n"
"	device const QuadInstance &quad = instances[instanceID];\n"
"	float2 pos = quad.positions[vertexID];\n"
"	float2 uv = quad.uvs[vertexID];\n"
"	uint color = quad.colors[vertexID];\n"
"\n"
"	VertexOut out {\n"
"		.pos = float4(floor(pos + 0.5f) * (float2(2.0f, -2.0f) / u.screen.xy) + float2(-1.0f, 1.0f), 0.0f, 1.0f),\n"
"		.uv = uv,\n"
"		.color = unpack_unorm4x8_to_float(color),\n"
"		.textureIndex = quad.textureIndex,\n"
"	};\n"
"	out.pos.y *= -1.0f;\n"
"	return out;\n"
"}\n"
"\n"
"fragment float4 fragment_main(VertexOut in [[stage_in]],\n"
"							  constant Arguments &arguments [[buffer(0)]])\n"
"{\n"
"	float4 tex_color = arguments.textures[in.textureIndex].sample(arguments.textureSampler, in.uv);\n"
"	float4 color = tex_color * in.color;\n"
"	return color;\n"
"}\n"
"\n"
"vertex VertexOut vertex_post(device const QuadInstance *instances [[buffer(0)]],\n"
"							  constant Uniforms &u           [[buffer(1)]],\n"
"							  uint instanceID                [[instance_id]],\n"
"							  uint vertexID                  [[vertex_id]])\n"
"{\n"
"	device const QuadInstance &quad = instances[instanceID];\n"
"	float2 pos = quad.positions[vertexID];\n"
"	float2 uv = quad.uvs[vertexID];\n"
"\n"
"	VertexOut out {\n"
"		.pos = float4(pos * (float2(2.0f, -2.0f) / u.screen.xy) + float2(-1.0f, 1.0f), 0.0f, 1.0f),\n"
"		.uv = uv,\n"
"		.color = float4(1.0f),\n"
"		.textureIndex = 0\n"
"	};\n"
"	out.pos.y *= -1.0f;\n"
"	return out;\n"
"}\n"
"\n"
"fragment float4 fragment_post_default(VertexOut in [[stage_in]],\n"
"									  constant Arguments &arguments [[buffer(0)]])\n"
"{\n"
"	return arguments.textures[in.textureIndex].sample(arguments.textureSampler, in.uv);\n"
"}\n"
"\n"
"// CRT effect based on https://www.shadertoy.com/view/Ms23DR\n"
"// by https://github.com/mattiasgustavsson/\n"
"\n"
"static float2 curve(float2 uv) {\n"
"	uv = (uv - 0.5f) * 2.0f;\n"
"	uv *= 1.1f;\n"
"	uv.x *= 1.0f + powr((abs(uv.y) / 5.0f), 2.0f);\n"
"	uv.y *= 1.0f + powr((abs(uv.x) / 4.0f), 2.0f);\n"
"	uv  = (uv / 2.0f) + 0.5f;\n"
"	uv =  uv * 0.92f + 0.04f;\n"
"	return uv;\n"
"}\n"
"\n"
"fragment float4 fragment_post_crt(VertexOut in                  [[stage_in]],\n"
"								  constant Arguments &arguments [[buffer(0)]],\n"
"								  constant Uniforms &u          [[buffer(1)]])\n"
"{\n"
"	auto screenbuffer = arguments.textures[in.textureIndex];\n"
"	auto screenSampler = arguments.textureSampler;\n"
"\n"
"	float2 uv = curve(in.uv);\n"
"	float3 color;\n"
"	float x = sin(0.3f * u.time + in.uv.y * 21.0f) * sin(0.7f * u.time + uv.y * 29.0f) *\n"
"	sin(0.3f + 0.33f * u.time + uv.y * 31.0f) * 0.0017f;\n"
"\n"
"	color.r = screenbuffer.sample(screenSampler, float2(x + uv.x + 0.001f, uv.y + 0.001f)).x + 0.05f;\n"
"	color.g = screenbuffer.sample(screenSampler, float2(x + uv.x + 0.000f, uv.y - 0.002f)).y + 0.05f;\n"
"	color.b = screenbuffer.sample(screenSampler, float2(x + uv.x - 0.002f, uv.y + 0.000f)).z + 0.05f;\n"
"	color.r += 0.08 * screenbuffer.sample(screenSampler, 0.75f * float2(x + 0.025f, -0.027f) + float2(uv.x + 0.001f, uv.y + 0.001f)).x;\n"
"	color.g += 0.05 * screenbuffer.sample(screenSampler, 0.75f * float2(x - 0.022f, -0.020f) + float2(uv.x + 0.000f, uv.y - 0.002f)).y;\n"
"	color.b += 0.08 * screenbuffer.sample(screenSampler, 0.75f * float2(x + -0.02f, -0.018f) + float2(uv.x - 0.002f, uv.y + 0.000f)).z;\n"
"\n"
"	color = saturate(color * 0.6f + 0.4f * color * color * 1.0f);\n"
"\n"
"	float vignette = (0.0f + 1.0f * 16.0f * uv.x * uv.y * (1.0f - uv.x) * (1.0f - uv.y));\n"
"	color *= float3(powr(vignette, 0.25f));\n"
"	color *= float3(0.95f, 1.05f, 0.95f);\n"
"	color *= 2.8;\n"
"\n"
"	float scanlines = saturate(0.35f + 0.35f * sin(3.5f * u.time + uv.y * u.screen.y * 1.5f));\n"
"	float s = powr(scanlines, 1.7f);\n"
"	color = color * float3(0.4f + 0.7f * s);\n"
"\n"
"	color *= 1.0f + 0.01f * sin(110.0f * u.time);\n"
"	if (uv.x < 0.0f || uv.x > 1.0f) {\n"
"		color *= 0.0;\n"
"	}\n"
"	if (uv.y < 0.0f || uv.y > 1.0f) {\n"
"		color *= 0.0;\n"
"	}\n"
"\n"
"	color *= 1.0f - 0.65f * float3(saturate((fmod(in.pos.x, 2.0f) - 1.0f) * 2.0f));\n"
"	return float4(color, 1.0f);\n"
"}\n";

typedef struct {
	simd_float2 screen;
	simd_float2 fade;
	float time;
} shader_uniforms_t;

typedef struct {
	MTLResourceID textures[RENDER_TEXTURES_MAX];
	MTLResourceID sampler;
} shader_arguments_t;

typedef struct __attribute__((aligned(RENDER_INSTANCE_ALIGNMENT))) {
	simd_float2 positions[4];
	simd_float2 uvs[4];
	uint32_t colors[4];
	uint32_t textureIndex;
} quad_instance_t;

// レンダリング関連の定義と変数
// =============================================================================
// 【初心者向け解説】
// ここからは、Metal APIを使用した実際のレンダリング処理に関する定義や変数が
// 含まれています。これらは、テクスチャ管理、描画命令の発行、画面への表示などを
// 制御します。

// 特殊テクスチャハンドル
texture_t RENDER_NO_TEXTURE;             // テクスチャなし（白テクスチャ）
static texture_t RENDER_BACKBUFFER_TEXTURE;  // バックバッファテクスチャ

// ブレンドモードの数
// -----------------------------------------------------------------------------
// ブレンドモード数と列挙型の最大値を同期させる
#define RENDER_BLEND_COUNT (RENDER_BLEND_LIGHTER + 1)

// 同時処理可能なフレーム数
// -----------------------------------------------------------------------------
// 【初心者向け解説】
// GPUは非同期で動作するため、CPUが次のフレームを準備している間に
// GPUが前のフレームを処理できるよう、複数のフレームを同時に扱います。
// これにより、CPUとGPUのリソースを最大限に活用できます。
#define MAX_FRAMES_IN_FLIGHT 3  // 同時に3フレームまで処理可能

// Metalコンテキスト構造体
// -----------------------------------------------------------------------------
// 【初心者向け解説】
// この構造体は、Metal APIに関連するすべてのオブジェクトを格納します。
// デバイス、コマンドキュー、シェーダーライブラリ、パイプライン状態など、
// 描画に必要なすべてのリソースがここに含まれます。
typedef struct {
	id<MTLDevice> device;              // Metalデバイス（GPU）
	id<MTLCommandQueue> commandQueue;  // コマンドキュー（GPU命令の送信用）
	id<MTLLibrary> library;            // シェーダーライブラリ
	
	// レンダリングパイプライン（シェーダープログラムと状態のセット）
	id<MTLRenderPipelineState> mainRenderPipelines[RENDER_BLEND_COUNT];  // 通常描画用
	id<MTLRenderPipelineState> postRenderPipelines[RENDER_POST_MAX];     // ポストエフェクト用
	
	// リソース管理
	id<MTLTexture> textures[RENDER_TEXTURES_MAX];                    // テクスチャ配列
	id<MTLSamplerState> sampler;                                     // テクスチャサンプラー
	id<MTLBuffer> instanceBuffers[MAX_FRAMES_IN_FLIGHT];             // インスタンスデータバッファ
	id<MTLBuffer> argumentBuffer;                                    // 引数バッファ
	
	// コマンド管理
	id<MTLCommandBuffer> currentCommandBuffer;                       // 現在のコマンドバッファ
	dispatch_semaphore_t frameSemaphore;                            // フレーム同期用セマフォ
} mtl_ctxt_t;

static mtl_ctxt_t mtl;

static const MTLPixelFormat atlasFormat = MTLPixelFormatRGBA8Unorm;
static const MTLPixelFormat renderbufferFormat = MTLPixelFormatBGRA8Unorm;

static const BOOL useMipmaps = RENDER_USE_MIPMAPS ? YES : NO;
static vec2i_t screenSize;
static vec2i_t backbufferSize;
static BOOL reencodeArgumentBuffer = YES;
static BOOL clearBackbuffer = NO;
static size_t textureCount;
static size_t frameIndex;
static size_t maxInstanceCount;
static size_t instanceCount;
static size_t instanceBufferLength;
static size_t instanceBufferReadOffset;
static size_t instanceBufferWriteOffset;
static render_blend_mode_t blendMode = RENDER_BLEND_NORMAL;
static size_t postEffectIndex = 0;

static uint32_t alignup(uint32_t n, uint32_t alignment) {
	return ((n + alignment - 1) / alignment) * alignment;
}

static void render_realloc_instance_storage(size_t newMaxInstanceCount) {
	size_t alignedInstanceSize = alignup(sizeof(quad_instance_t), RENDER_INSTANCE_ALIGNMENT);
	// When performing instancing, instance data must be packed (allowing for padding
	// inside instance structs but not between them). Hence the aligned size actually
	// has to be the size of the struct.
	error_if(sizeof(quad_instance_t) != alignedInstanceSize,
			 "Instance size must be exactly equal to aligned instance size");
	if (newMaxInstanceCount > maxInstanceCount) {
		size_t bufferLength = alignedInstanceSize * newMaxInstanceCount;
		size_t currentBufferIndex = frameIndex % MAX_FRAMES_IN_FLIGHT;
		id<MTLBuffer> inProgressBuffer = mtl.instanceBuffers[currentBufferIndex];
		for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
			mtl.instanceBuffers[i] = [mtl.device newBufferWithLength:bufferLength
															 options:MTLResourceStorageModeShared];
		}
		if (inProgressBuffer) {
			// If we're mid-frame, we need to preserve buffer contents because there might be instances
			// that have been copied but not yet encoded, within the range [instanceBufferReadOffset,
			// instanceBufferWriteOffset).
			memcpy([mtl.instanceBuffers[currentBufferIndex] contents],
				   [inProgressBuffer contents],
				   inProgressBuffer.length);
		}
		instanceBufferLength = bufferLength;
		maxInstanceCount = newMaxInstanceCount;
	}
}

static quad_instance_t *render_alloc_quads(size_t count) {
	size_t alignedInstanceSize = alignup(sizeof(quad_instance_t), RENDER_INSTANCE_ALIGNMENT);
	while (instanceBufferWriteOffset + alignedInstanceSize * count >= instanceBufferLength) {
		render_realloc_instance_storage(maxInstanceCount * 2);
	}
	size_t bufferIndex = frameIndex % MAX_FRAMES_IN_FLIGHT;
	id<MTLBuffer> currentInstanceBuffer = mtl.instanceBuffers[bufferIndex];
	size_t instanceOffset = instanceBufferWriteOffset;
	instanceBufferWriteOffset += alignedInstanceSize * count;
	return (quad_instance_t *)([currentInstanceBuffer contents] + instanceOffset);
}

// Metalレンダラーバックエンドの初期化
// -----------------------------------------------------------------------------
// 【初心者向け解説】
// この関数は、Metalレンダラーの初期化を行います。アプリケーション起動時に
// 一度だけ呼び出され、描画に必要なすべてのリソースを準備します。
//
// Metal APIは、Apple製品のためのモダンなグラフィックスAPIで、
// 従来のOpenGLよりも低レベルで効率的なハードウェアアクセスを提供します。
void render_backend_init(void) {
	// Metalデバイス（GPU）の取得とコマンドキューの作成
	// -------------------------
	// 【初心者向け解説】
	// Metalデバイスは物理的なGPUを表し、コマンドキューはGPUに送信する命令の列です。
	mtl.device = MTLCreateSystemDefaultDevice();  // システムのデフォルトGPUを取得
	mtl.commandQueue = [mtl.device newCommandQueue];  // コマンドキューを作成
	mtl.currentCommandBuffer = nil;  // コマンドバッファは最初はなし

	// バインドレスレンダリングのサポートチェック
	// -------------------------
	// 【初心者向け解説】
	// 「バインドレスレンダリング」とは、多数のテクスチャを効率的に切り替える
	// 高度な機能で、このレンダラーには必要です。
	BOOL supportsBindless = mtl.device.argumentBuffersSupport == MTLArgumentBuffersTier2;
	error_if(!supportsBindless, "Metal renderer requires support for argument buffers tier 2");

	// 描画レイヤーの設定
	// -------------------------
	// 【初心者向け解説】
	// CAMetalLayerは、Metalで描画した内容を画面に表示するための特殊なレイヤーです。
	CAMetalLayer *layer = (__bridge CAMetalLayer *)platform_get_metal_layer();
	layer.device = mtl.device;  // 使用するGPUを指定
	layer.pixelFormat = renderbufferFormat;  // ピクセル形式を設定（BGRA8）

	// シェーダーライブラリのコンパイル
	// -------------------------
	// 【初心者向け解説】
	// シェーダーとは、GPUで実行される小さなプログラムです。
	// ここでは、シェーダーのソースコードをコンパイルしてライブラリとして読み込みます。
	MTLCompileOptions *options = [MTLCompileOptions new];  // コンパイルオプションを作成
	options.preprocessorMacros = @{  // プリプロセッサマクロ（定数）を定義
		@"RENDER_TEXTURES_MAX" : @(RENDER_TEXTURES_MAX),
		@"RENDER_INSTANCE_ALIGNMENT" : @(RENDER_INSTANCE_ALIGNMENT)
	};
	NSError *error = nil;
	// シェーダーソースからライブラリを作成
	mtl.library = [mtl.device newLibraryWithSource:[NSString stringWithUTF8String:shaderSource]
										   options:options
											 error:&error];
	// エラーチェック
	error_if(error != nil, "Error occurred when creating library: %s",
			 [error.localizedDescription cStringUsingEncoding:NSUTF8StringEncoding]);

	// レンダリングパイプラインの作成
	// -------------------------
	// 【初心者向け解説】
	// パイプラインは、頂点処理、ラスタライズ、フラグメント処理などを含む
	// 描画処理の全体的な流れを定義します。
	MTLRenderPipelineDescriptor *pipelineDescriptor = [MTLRenderPipelineDescriptor new];
	
	// 頂点シェーダーとフラグメントシェーダーを設定
	pipelineDescriptor.vertexFunction = [mtl.library newFunctionWithName:@"vertex_main"];
	pipelineDescriptor.fragmentFunction = [mtl.library newFunctionWithName:@"fragment_main"];
	
	// 出力形式とブレンド設定
	pipelineDescriptor.colorAttachments[0].pixelFormat = renderbufferFormat;
	pipelineDescriptor.colorAttachments[0].blendingEnabled = YES;  // 透明度対応

	// 各ブレンドモード用のパイプラインを作成
	// -------------------------
	// 【初心者向け解説】
	// ブレンドモードは、新しいピクセルと既存のピクセルをどのように混合するかを定義します。
	// 例：通常ブレンド（透明度）、加算ブレンド（光や発光効果）など
	for (int blendMode = 0; blendMode < RENDER_BLEND_COUNT; ++blendMode) {
		switch (blendMode) {
			case RENDER_BLEND_NORMAL:  // 通常ブレンド（透明度）
				pipelineDescriptor.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
				pipelineDescriptor.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
				pipelineDescriptor.colorAttachments[0].rgbBlendOperation = MTLBlendOperationAdd;
				pipelineDescriptor.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorSourceAlpha;
				pipelineDescriptor.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
				pipelineDescriptor.colorAttachments[0].alphaBlendOperation = MTLBlendOperationAdd;
				break;
			case RENDER_BLEND_LIGHTER:  // 加算ブレンド（光/発光効果）
				pipelineDescriptor.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
				pipelineDescriptor.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOne;
				pipelineDescriptor.colorAttachments[0].rgbBlendOperation = MTLBlendOperationAdd;
				pipelineDescriptor.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorSourceAlpha;
				pipelineDescriptor.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOne;
				pipelineDescriptor.colorAttachments[0].alphaBlendOperation = MTLBlendOperationAdd;
				break;
		}

		// パイプライン状態オブジェクトを作成
		mtl.mainRenderPipelines[blendMode] = [mtl.device newRenderPipelineStateWithDescriptor:pipelineDescriptor error:&error];
		error_if(error != nil, "Error occurred when creating render pipeline: %s",
				 [error.localizedDescription cStringUsingEncoding:NSUTF8StringEncoding]);
	}

	// ポストエフェクトパイプラインの作成
	// -------------------------
	// 【初心者向け解説】
	// ポストエフェクトは、通常の描画が完了した後に画面全体に適用される視覚効果です。
	// 例：CRTエフェクト（古いテレビ風）、ぼかし、色調補正など
	
	// ポストエフェクトはブレンドが不要
	pipelineDescriptor.colorAttachments[0].blendingEnabled = NO;

	// 標準（エフェクトなし）ポストエフェクト
	pipelineDescriptor.vertexFunction = [mtl.library newFunctionWithName:@"vertex_post"];
	pipelineDescriptor.fragmentFunction = [mtl.library newFunctionWithName:@"fragment_post_default"];
	mtl.postRenderPipelines[RENDER_POST_NONE] = [mtl.device newRenderPipelineStateWithDescriptor:pipelineDescriptor error:&error];
	error_if(error != nil, "Error occurred when creating render pipeline: %s",
			 [error.localizedDescription cStringUsingEncoding:NSUTF8StringEncoding]);

	// CRT（ブラウン管）エフェクト
	pipelineDescriptor.vertexFunction = [mtl.library newFunctionWithName:@"vertex_post"];
	pipelineDescriptor.fragmentFunction = [mtl.library newFunctionWithName:@"fragment_post_crt"];
	mtl.postRenderPipelines[RENDER_POST_CRT] = [mtl.device newRenderPipelineStateWithDescriptor:pipelineDescriptor error:&error];
	error_if(error != nil, "Error occurred when creating render pipeline: %s",
			 [error.localizedDescription cStringUsingEncoding:NSUTF8StringEncoding]);

	// バックバッファテクスチャの予約
	// -------------------------
	// 【初心者向け解説】
	// バックバッファは、画面に表示する前に描画を行う一時的なテクスチャです。
	// これにより、画面のちらつきを防ぎ、ポストエフェクトの適用も可能になります。
	RENDER_BACKBUFFER_TEXTURE = (texture_t){ .index = textureCount++ };

	// テクスチャサンプラーの作成
	// -------------------------
	// 【初心者向け解説】
	// サンプラーは、テクスチャから色を取得する方法を定義します。
	// フィルタリング（補間）や繰り返し設定などを含みます。
	MTLSamplerDescriptor *samplerDescriptor = [MTLSamplerDescriptor new];
	samplerDescriptor.sAddressMode = MTLSamplerAddressModeClampToEdge;  // 端で繰り返さない
	samplerDescriptor.tAddressMode = MTLSamplerAddressModeClampToEdge;  // 端で繰り返さない
	samplerDescriptor.minFilter = MTLSamplerMinMagFilterLinear;         // 縮小時は補間
	samplerDescriptor.magFilter = MTLSamplerMinMagFilterNearest;        // 拡大時はピクセル化
	samplerDescriptor.mipFilter = RENDER_USE_MIPMAPS ? MTLSamplerMipFilterLinear : MTLSamplerMipFilterNotMipmapped;
	samplerDescriptor.supportArgumentBuffers = YES;  // 引数バッファでの使用をサポート
	mtl.sampler = [mtl.device newSamplerStateWithDescriptor:samplerDescriptor];

	// インスタンスバッファの初期割り当て
	// -------------------------
	// 【初心者向け解説】
	// 頂点データを格納するためのメモリ領域を確保します。
	render_realloc_instance_storage(RENDER_BUFFER_CAPACITY);

	// 引数バッファの作成
	// -------------------------
	// 【初心者向け解説】
	// 引数バッファは、多数のテクスチャをシェーダーに効率的に渡すための
	// メカニズムです。
	mtl.argumentBuffer = [mtl.device newBufferWithLength:sizeof(shader_arguments_t)
												 options:MTLResourceStorageModeShared];

	// 白テクスチャの作成（テクスチャなしの場合に使用）
	// -------------------------
	rgba_t white_pixels[4] = {rgba_white(), rgba_white(), rgba_white(), rgba_white()};
	RENDER_NO_TEXTURE = texture_create(vec2i(2, 2), white_pixels);

	// フレーム同期用セマフォの作成
	// -------------------------
	// 【初心者向け解説】
	// セマフォは同時実行されるフレーム数を制限し、GPU処理の完了を待機するための
	// 同期メカニズムです。
	mtl.frameSemaphore = dispatch_semaphore_create(MAX_FRAMES_IN_FLIGHT);
}

void render_backend_cleanup(void) {
	mtl.sampler = nil;
	for (int i = 0; i < textureCount; ++i) {
		mtl.textures[i] = nil;
	}
	textureCount = 0;
	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
		mtl.instanceBuffers[i] = nil;
	}
	mtl.argumentBuffer = nil;
	for (int i = 0; i < RENDER_POST_MAX; ++i) {
		mtl.postRenderPipelines[i] = nil;
	}
	for (int i = 0; i < RENDER_BLEND_COUNT; ++i) {
		mtl.mainRenderPipelines[i] = nil;
	}
	mtl.library = nil;
	mtl.commandQueue = nil;
	mtl.device = nil;
}

static void render_flush(id<MTLRenderPipelineState> renderPipeline, id<MTLTexture> destinationTexture)
{
	if (reencodeArgumentBuffer) {
		shader_arguments_t *args = (shader_arguments_t *)[mtl.argumentBuffer contents];
		for (int i = 0; i < textureCount; ++i) {
			args->textures[i] = mtl.textures[i].gpuResourceID;
		}
		args->sampler = mtl.sampler.gpuResourceID;
		reencodeArgumentBuffer = NO;
	}

	MTLRenderPassDescriptor *passDescriptor = [MTLRenderPassDescriptor new];
	passDescriptor.colorAttachments[0].texture = destinationTexture;
	passDescriptor.colorAttachments[0].loadAction = clearBackbuffer ? MTLLoadActionClear : MTLLoadActionLoad;
	passDescriptor.colorAttachments[0].storeAction = MTLStoreActionStore;
	passDescriptor.colorAttachments[0].clearColor = MTLClearColorMake(0, 0, 0, 1);
	clearBackbuffer = NO; // Accumulate multiple passes if needed due to quad buffer size

	id<MTLRenderCommandEncoder> renderCommandEncoder = [mtl.currentCommandBuffer renderCommandEncoderWithDescriptor:passDescriptor];

	[renderCommandEncoder setFrontFacingWinding:MTLWindingClockwise];
	[renderCommandEncoder setCullMode:MTLCullModeBack];

	MTLViewport viewport = { 0, 0, backbufferSize.x, backbufferSize.y, 0, 1 };
	[renderCommandEncoder setViewport:viewport];

	[renderCommandEncoder setRenderPipelineState:renderPipeline];
	[renderCommandEncoder useResources:mtl.textures count:textureCount usage:MTLResourceUsageRead];

	shader_uniforms_t uniforms = {
		simd_make_float2(screenSize.x, screenSize.y),
		simd_make_float2(0.0f, 0.0f),
		engine.time
	};
	size_t bufferIndex = frameIndex % MAX_FRAMES_IN_FLIGHT;
	[renderCommandEncoder setVertexBuffer:mtl.instanceBuffers[bufferIndex] offset:instanceBufferReadOffset atIndex:0];
	[renderCommandEncoder setVertexBytes:&uniforms length:sizeof(shader_uniforms_t) atIndex:1];
	[renderCommandEncoder setFragmentBuffer:mtl.argumentBuffer offset:0 atIndex:0];
	[renderCommandEncoder setFragmentBytes:&uniforms length:sizeof(shader_uniforms_t) atIndex:1];

	if (instanceCount != 0) {
		[renderCommandEncoder drawPrimitives:MTLPrimitiveTypeTriangleStrip
								 vertexStart:0
								 vertexCount:4
							   instanceCount:instanceCount
								baseInstance:0];
		instanceBufferReadOffset = instanceBufferWriteOffset;
		instanceCount = 0;
	}

	[renderCommandEncoder endEncoding];
}

void render_set_screen(vec2i_t size) {
	screenSize = size;
	backbufferSize = size;
	MTLTextureDescriptor *descriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:renderbufferFormat
																						  width:backbufferSize.x
																						 height:backbufferSize.y
																					  mipmapped:NO];
	descriptor.storageMode = MTLStorageModePrivate;
	descriptor.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
	id<MTLTexture> backbufferTexture = [mtl.device newTextureWithDescriptor:descriptor];
	mtl.textures[RENDER_BACKBUFFER_TEXTURE.index] = backbufferTexture;
	reencodeArgumentBuffer = YES;
}

void render_set_blend_mode(render_blend_mode_t mode) {
	if (mode == blendMode) {
		return;
	}
	render_flush(mtl.mainRenderPipelines[blendMode], mtl.textures[RENDER_BACKBUFFER_TEXTURE.index]);
	blendMode = mode;
}

void render_set_post_effect(render_post_effect_t post) {
	error_if(post < 0 || post > RENDER_POST_MAX, "Invalid post effect %d", post);
	postEffectIndex = post;
}

void render_frame_prepare(void) {
	@autoreleasepool {
		dispatch_semaphore_wait(mtl.frameSemaphore, 1 * NSEC_PER_SEC);
		mtl.currentCommandBuffer = [mtl.commandQueue commandBuffer];
		clearBackbuffer = YES;
		instanceBufferReadOffset = 0;
		instanceBufferWriteOffset = 0;
	}
}

void render_frame_end(void) {
	@autoreleasepool {
		// Main Pass
		render_flush(mtl.mainRenderPipelines[blendMode], mtl.textures[RENDER_BACKBUFFER_TEXTURE.index]);

		// Post Pass and Present
		CAMetalLayer *layer = (__bridge CAMetalLayer *)platform_get_metal_layer();
		id<CAMetalDrawable> drawable = [layer nextDrawable];
		if (drawable) {
			quad_instance_t *instance = render_alloc_quads(1);
			quad_instance_t quad = {
				.positions = {{0, 0}, {0, screenSize.y}, {screenSize.x, 0}, {screenSize.x, screenSize.y}},
				.uvs = {{0, 0},{0, 1}, {1, 0}, {1, 1}},
				.colors = {rgba_white().v, rgba_white().v, rgba_white().v, rgba_white().v},
				.textureIndex = RENDER_BACKBUFFER_TEXTURE.index
			};
			memcpy(instance, &quad, sizeof(quad_instance_t));
			++instanceCount;

			clearBackbuffer = YES;
			render_flush(mtl.postRenderPipelines[postEffectIndex], drawable.texture);

			[mtl.currentCommandBuffer presentDrawable:drawable];
		}

		[mtl.currentCommandBuffer addCompletedHandler:^(id<MTLCommandBuffer> commandBuffer) {
			dispatch_semaphore_signal(mtl.frameSemaphore);
		}];

		[mtl.currentCommandBuffer commit];
		mtl.currentCommandBuffer = nil;
	}
	++frameIndex;
}

void render_draw_quad(quadverts_t *quad, texture_t texture_handle) {
	error_if(texture_handle.index >= textureCount, "Invalid texture %d", texture_handle.index);
	id<MTLTexture> texture = mtl.textures[texture_handle.index];
	quad_instance_t *instance = render_alloc_quads(1);
	int reorder[] = { 1, 0, 2, 3 }; // Swaps vertices so they can be drawn as triangle strips
	for (uint32_t i = 0; i < 4; i++) {
		instance->positions[reorder[i]] = (simd_float2){quad->vertices[i].pos.x, quad->vertices[i].pos.y};
		instance->uvs[reorder[i]].x = quad->vertices[i].uv.x / texture.width;
		instance->uvs[reorder[i]].y = quad->vertices[i].uv.y / texture.height;
		instance->colors[reorder[i]] = quad->vertices[i].color.v;
	}
	instance->textureIndex = texture_handle.index;
	++instanceCount;
}

// -----------------------------------------------------------------------------
// Textures

texture_mark_t textures_mark(void) {
	return (texture_mark_t){.index = textureCount };
}

void textures_reset(texture_mark_t mark) {
	error_if(mark.index > textureCount, "Invalid texture reset mark %d >= %d", mark.index, textureCount);
	error_if(mark.index < 2, "Invalid texture reset mark %d < %d", mark.index, 2);
	if (mark.index == textureCount) {
		return;
	}
	render_flush(mtl.mainRenderPipelines[blendMode], mtl.textures[RENDER_BACKBUFFER_TEXTURE.index]);
	for (int i = mark.index; i < textureCount; ++i) {
		mtl.textures[i] = nil;
	}
	textureCount = mark.index;

	reencodeArgumentBuffer = YES;
}

static void texture_generate_mipmaps(id<MTLTexture> texture) {
	if (useMipmaps) {
		id<MTLCommandBuffer> commandBuffer = [mtl.commandQueue commandBuffer];
		id<MTLBlitCommandEncoder> mipmapEncoder = [commandBuffer blitCommandEncoder];
		[mipmapEncoder generateMipmapsForTexture:texture];
		[mipmapEncoder endEncoding];
		[commandBuffer commit];
	}
}

texture_t texture_create(vec2i_t size, rgba_t *pixels) {
	error_if(textureCount >= RENDER_TEXTURES_MAX, "RENDER_TEXTURES_MAX reached");
	MTLTextureDescriptor *descriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:atlasFormat
																						  width:size.x
																						 height:size.y
																					  mipmapped:useMipmaps];
	descriptor.usage = MTLTextureUsageShaderRead;
	if (mtl.device.hasUnifiedMemory) {
		descriptor.storageMode = MTLResourceStorageModeShared;
	}
	id<MTLTexture> texture = [mtl.device newTextureWithDescriptor:descriptor];
	[texture replaceRegion:MTLRegionMake2D(0, 0, size.x, size.y) mipmapLevel:0 withBytes:pixels bytesPerRow:size.x * 4];
	texture_t texture_handle = {.index = textureCount};
	mtl.textures[textureCount] = texture;
	++textureCount;
	texture_generate_mipmaps(texture);
	reencodeArgumentBuffer = YES;
	return texture_handle;
}

void texture_replace_pixels(texture_t texture_handle, vec2i_t size, rgba_t *pixels) {
	error_if(texture_handle.index >= textureCount, "Invalid texture %d", texture_handle.index);
	id<MTLTexture> texture = mtl.textures[texture_handle.index];
	error_if(texture.width < size.x || texture.height < size.y,
			 "Cannot replace %dx%d pixels of %dx%d texture", size.x, size.y, texture.width, texture.height);
	[texture replaceRegion:MTLRegionMake2D(0, 0, size.x, size.y) mipmapLevel:0 withBytes:pixels bytesPerRow:size.x * 4];
	texture_generate_mipmaps(texture);
}
