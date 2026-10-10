'use strict';
const {test} = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const root = path.resolve(__dirname, '..');
const read = name => fs.readFileSync(path.join(root, name), 'utf8');

test('Compressor explicitly opts into separate ST/MT builds with a bounded worker budget', () => {
  const env = read('profiles/video-compressor/profile.env');
  assert.match(env, /PROFILE_THREADING_VARIANTS="single-thread,multi-thread"/);
  for (const [key, value] of Object.entries({PTHREAD_POOL_SIZE:8, DECODER_THREAD_COUNT:2, ENCODER_THREAD_COUNT:4, X264_LOOKAHEAD_THREAD_COUNT:1}))
    assert.match(env, new RegExp(`PROFILE_${key}=${value}\\b`));
});
test('libvpx and Opus Docker stages receive the selected threading mode', () => {
  const docker = read('docker/Dockerfile');
  for (const stage of ['libvpx', 'libopus']) {
    const section = docker.split(`AS ${stage}-builder`)[1].split('\nFROM ')[0];
    assert.match(section, /ARG THREADING_MODE=single-thread/);
    assert.match(section, /THREADING_MODE=\$\{THREADING_MODE\}/);
  }
});
test('libvpx keeps ST disabled and explicitly builds pthread VP9 for MT', () => {
  const script = read('scripts/build-libvpx.sh');
  assert.match(script, /THREADING_MODE/);
  assert.match(script, /--enable-multithread/);
  assert.match(script, /--disable-multithread/);
  assert.match(script, /-pthread/);
});
test('Opus is compiled compatibly with shared-memory MT linking', () => {
  assert.match(read('scripts/build-libopus.sh'), /CMAKE_C_FLAGS=.*thread_flags/);
});
test('runner preserves ST but uses bounded video decoding and encoding for MT', () => {
  const c = read('runners/video-compressor.c');
  assert.match(c, /thread_count = FFMPEG_WASM_DECODER_THREAD_COUNT/);
  assert.match(c, /thread_count = FFMPEG_WASM_PTHREADS \? FFMPEG_WASM_ENCODER_THREAD_COUNT : 1/);
  assert.match(c, /"row-mt", "1"/);
  assert.match(c, /lookahead-threads=%d/);
  assert.match(c, /FFMPEG_WASM_PTHREADS && input_stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO/);
  const audio = c.split('static int setup_audio_output')[1].split('static int ')[0];
  assert.match(audio, /thread_count = 1/);
});
test('browser smoke selects the actual variant and verifies both encoder thread logs', () => {
  const smoke = read('tests/smoke-tests/video-compressor.js');
  assert.match(smoke, /loadEmbedded\(\{ coreJsText: observedCoreJsText, wasmBytes, threading: threadingMode \}\)/);
  assert.match(smoke, /assertThreading\('libx264'\)/);
  assert.match(smoke, /assertThreading\('libvpx-vp9'\)/);
});
test('release keeps legacy ST alias and publishes both explicit Compressor variants', () => {
  const release = read('.github/workflows/release.yml');
  for (const suffix of ['', '-single-thread', '-multi-thread'])
    assert.ok(release.includes(`ffmpeg-wasm-video-compressor${suffix}-v`));
  const packer = read('scripts/prepare-release.sh');
  assert.match(packer, /cp "\$RELEASE_DIR\/ffmpeg-wasm-video-compressor-single-thread-v\$\{BUILDER_VERSION\}\.zip"/);
});
test('filter graphs cannot auto-spawn beyond the fixed pthread budget', () => {
  assert.match(read('runners/video-compressor.c'), /if \(graph && FFMPEG_WASM_PTHREADS\) graph->nb_threads = 1/);
});
test('portable Builder demo continues to select the ST subdirectory', () => {
  const pack = read('scripts/pack-single-html.ps1');
  assert.match(pack, /Join-Path \$DistDir "single-thread"/);
  assert.match(pack, /\$DistDir = \$SingleThreadDir/);
});
test('aggregate source validation expects the same Builder version', () => {
  const version = /^BUILDER_VERSION=(.+)$/m.exec(read('versions.env'))[1];
  assert.ok(read('scripts/check-repository.ps1').includes(`Builder version must be ${version}.`));
});
test('aggregate validation accepts the actual Compressor runner version',()=>{
 const version=/#define RUNNER_VERSION "([^"]+)"/.exec(read('runners/video-compressor.c'))[1];
 assert.ok(read('scripts/check-repository.ps1').includes(`Video runner version must be ${version}.`));
});
test('libvpx uses its supported linker environment instead of an unknown configure flag',()=>{
 const s=read('scripts/build-libvpx.sh');
 assert.doesNotMatch(s,/--extra-ldflags/);
 assert.match(s,/export LDFLAGS=.*thread_flags/);
});
test('browser smoke observes shared memory and real nested Worker construction',()=>{
 const smoke=read('tests/smoke-tests/video-compressor.js');
 assert.match(smoke,/WebAssembly\.Memory/);
 assert.match(smoke,/extends OriginalWorker/);
 assert.match(smoke,/__COMPRESSOR_PTHREAD_CREATED__/);
 assert.match(smoke,/assertRuntimeObserved\(\)/);
});
test('browser smoke cancels a live core and transcodes again without audio',()=>{
 const smoke=read('tests/smoke-tests/video-compressor.js');
 assert.match(smoke,/new AbortController\(\)/);
 assert.match(smoke,/runner.activeRuns.size !== 0/);
 assert.match(smoke,/retryBytes/);
 assert.match(smoke,/noAudio: true/);
});
