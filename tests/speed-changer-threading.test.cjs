'use strict';
const {test} = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const {spawnSync} = require('node:child_process');
const root = path.resolve(__dirname, '..');
const read = name => fs.readFileSync(path.join(root, name), 'utf8');

test('Speed Changer explicitly opts into ST/MT with the bounded 8/2/4/1 budget', () => {
  const command = `source profiles/video-speed-changer/profile.env; printf '%s\\n' "$PROFILE_THREADING_VARIANTS" "$PROFILE_PTHREAD_POOL_SIZE" "$PROFILE_DECODER_THREAD_COUNT" "$PROFILE_ENCODER_THREAD_COUNT" "$PROFILE_X264_LOOKAHEAD_THREAD_COUNT" "$PROFILE_CAPABILITIES_JSON"`;
  const result = spawnSync('bash', ['-c', command], {cwd: root, encoding: 'utf8'});
  assert.equal(result.status, 0, result.stderr);
  const [variants, pool, decoder, encoder, lookahead, capabilities] = result.stdout.trimEnd().split('\n');
  assert.equal(variants, 'single-thread,multi-thread');
  assert.deepEqual([pool, decoder, encoder, lookahead], ['8', '2', '4', '1']);
  assert.deepEqual(JSON.parse(capabilities).threadingVariants, ['single-thread', 'multi-thread']);
});

test('actual C thread assignments keep ST, audio and filters bounded while enabling MT video', t => {
  if (spawnSync('cc', ['--version']).status !== 0) return t.skip('cc unavailable');
  const c = read('runners/video-speed-changer.c');
  const decoder = c.slice(c.indexOf('stream->dec_ctx->pkt_timebase ='), c.indexOf('ret = avcodec_open2(stream->dec_ctx'));
  const video = c.split('static int setup_video_output')[1].split('static int setup_audio_output')[0];
  const audio = c.split('static int setup_audio_output')[1].split('static int init_filter')[0];
  const assignments = section => section.match(/stream->enc_ctx->thread_count =[^;]+;\s*stream->enc_ctx->thread_type =[^;]+;/)[0];
  const graph = c.split('AVFilterGraph *graph = avfilter_graph_alloc();')[1].split('const AVFilter *buffersrc')[0];
  const directory = fs.mkdtempSync(path.join(os.tmpdir(), 'speed-threading-'));
  try {
    const source = `#include <assert.h>
#include <stddef.h>
#define AVMEDIA_TYPE_VIDEO 1
#define FF_THREAD_FRAME 1
#define FF_THREAD_SLICE 2
#ifndef FFMPEG_WASM_PTHREADS
#define FFMPEG_WASM_PTHREADS 0
#endif
#define FFMPEG_WASM_DECODER_THREAD_COUNT 2
#define FFMPEG_WASM_ENCODER_THREAD_COUNT 4
typedef struct { int codec_type; } Parameters;
typedef struct { int pkt_timebase, thread_count, thread_type, framerate; } Codec;
typedef struct { Parameters *codecpar; int time_base; } Input;
typedef struct { Codec *dec_ctx, *enc_ctx; } Stream;
typedef struct { int ifmt_ctx; } Context;
typedef struct { int nb_threads; } AVFilterGraph;
static int av_guess_frame_rate(int format, Input *input, void *unused) { (void)format; (void)input; (void)unused; return 24; }
static void decode(Context *ctx, Input *input_stream, Stream *stream) { ${decoder} }
static void encode_video(Stream *stream) { ${assignments(video)} }
static void encode_audio(Stream *stream) { ${assignments(audio)} }
int main(void) {
  Codec decoded = {0}, encoded = {0}; Stream stream = {&decoded, &encoded};
  Context context = {0}; Parameters parameters = {AVMEDIA_TYPE_VIDEO}; Input input = {&parameters, 1000};
  decode(&context, &input, &stream);
  assert(decoded.thread_count == (FFMPEG_WASM_PTHREADS ? 2 : 1));
  assert(decoded.thread_type == (FFMPEG_WASM_PTHREADS ? FF_THREAD_FRAME | FF_THREAD_SLICE : 0));
  parameters.codec_type = 2; decode(&context, &input, &stream);
  assert(decoded.thread_count == 1 && decoded.thread_type == 0);
  encode_video(&stream); assert(encoded.thread_count == (FFMPEG_WASM_PTHREADS ? 4 : 1));
  assert(encoded.thread_type == (FFMPEG_WASM_PTHREADS ? FF_THREAD_FRAME : 0));
  encode_audio(&stream); assert(encoded.thread_count == 1 && encoded.thread_type == 0);
  AVFilterGraph allocation = {0}, *graph = &allocation;
  ${graph}
  assert(graph->nb_threads == (FFMPEG_WASM_PTHREADS ? 1 : 0));
  return 0;
}`;
    const file = path.join(directory, 'test.c');
    fs.writeFileSync(file, source);
    for (const mt of [0, 1]) {
      const executable = path.join(directory, `test-${mt}`);
      const built = spawnSync('cc', ['-Wall', '-Wextra', '-Werror', '-std=c11', `-DFFMPEG_WASM_PTHREADS=${mt}`, file, '-o', executable], {encoding: 'utf8'});
      assert.equal(built.status, 0, built.stderr);
      const result = spawnSync(executable, [], {encoding: 'utf8'});
      assert.equal(result.status, 0, `threading=${mt}: ${result.stderr}`);
    }
  } finally { fs.rmSync(directory, {recursive: true, force: true}); }
});

test('MT x264 lookahead is bounded and runner logs identify configured video threads', () => {
  const c = read('runners/video-speed-changer.c');
  assert.match(c, /lookahead-threads=%d/);
  assert.match(c, /FFMPEG_WASM_X264_LOOKAHEAD_THREAD_COUNT/);
  assert.match(c, /opening %s encoder: threads=%d x264_lookahead_threads=%d/);
  assert.match(c, /opening %s decoder: threads=%d/);
});

test('browser smoke selects each runtime and observes actual memory and pthread Workers', () => {
  const smoke = read('tests/smoke-tests/video-speed-changer.js');
  assert.match(smoke, /loadEmbedded\(\{ coreJsText: observedCoreJsText, wasmBytes, threading: threadingMode \}\)/);
  assert.match(smoke, /instanceof WebAssembly\.Memory/);
  assert.match(smoke, /instanceof SharedArrayBuffer/);
  assert.match(smoke, /extends OriginalWorker/);
  assert.match(smoke, /workers !== 8 : workers !== 0/);
  assert.match(smoke, /assertRuntimeObserved\(\)/);
  assert.match(smoke, /assertThreading\(\)/);
});

test('smoke cancels only after core initialization and verifies successful fresh-core retry', () => {
  const smoke = read('tests/smoke-tests/video-speed-changer.js');
  assert.match(smoke, /entry.message.includes\('opening libx264 encoder:'\)\) abortController.abort\(\)/);
  assert.match(smoke, /runner.activeRuns.size !== 0/);
  assert.match(smoke, /await runRate\(1\.5, \{ labelSuffix: "after-cancel" \}\)/);
});

test('release publishes explicit speed ZIPs and BUILDINFO alongside legacy ST aliases', () => {
  const workflow = read('.github/workflows/release.yml');
  for (const suffix of ['', '-single-thread', '-multi-thread']) {
    assert.ok(workflow.includes(`ffmpeg-wasm-video-speed-changer${suffix}-v`), suffix);
    assert.ok(workflow.includes(`BUILDINFO-video-speed-changer${suffix}.txt`), suffix);
  }
  const packer = read('scripts/prepare-release.sh');
  assert.match(packer, /cp "\$RELEASE_DIR\/ffmpeg-wasm-video-speed-changer-single-thread-v\$\{BUILDER_VERSION\}\.zip"/);
  assert.match(packer, /cp "\$RELEASE_DIR\/BUILDINFO-video-speed-changer-single-thread\.txt"/);
  assert.ok(packer.includes('ffmpeg-wasm-sources-v${BUILDER_VERSION}.tar.gz'));
  assert.ok(packer.includes('"$X264_SRC/COPYING" "$binary_dir/LICENSES/x264-COPYING"'));
});
