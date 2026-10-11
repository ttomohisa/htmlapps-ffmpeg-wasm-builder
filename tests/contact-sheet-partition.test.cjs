'use strict';
const {test} = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const vm = require('node:vm');
const {spawnSync} = require('node:child_process');
const root = path.resolve(__dirname, '..');
const read = name => fs.readFileSync(path.join(root, name), 'utf8');
const context = {window: {}};
vm.runInNewContext(read('runtime/browser-ffmpeg.js'), context);
const args = options => Array.from(context.window.BrowserFFmpeg.videoContactSheetArgs(options));

function runtime(Worker) {
  const revoked = [];
  const sandbox = {Worker, WebAssembly, Blob, ArrayBuffer, Uint8Array, AbortController, DOMException,
    URL:{createObjectURL:()=>'blob:test',revokeObjectURL:url=>revoked.push(url)}};
  sandbox.window = sandbox;
  vm.runInNewContext(read('runtime/browser-ffmpeg.js'), sandbox);
  return {api:sandbox.BrowserFFmpeg,revoked};
}

test('contact-sheet runtime preserves legacy arguments unless a partition is requested', () => {
  assert.deepEqual(args({count:24}), ['--input','/workerfs/input.mp4','--output','/contact-sheet.ppm','--count','24','--thumb-size','320']);
  assert.deepEqual(args({count:48,partitionCount:2,partitionIndex:1}).slice(-4), ['--partition-count','2','--partition-index','1']);
  assert.deepEqual(args({partitionCount:2}).slice(-4), ['--partition-count','2','--partition-index','0']);
});

test('partition arguments reject out-of-budget, fractional and malformed values', () => {
  for (const options of [{partitionCount:0},{partitionCount:3},{partitionCount:1.5},{partitionCount:'2'},{partitionCount:2,partitionIndex:2},{partitionCount:2,partitionIndex:-1},{partitionCount:2,partitionIndex:0.5},{partitionIndex:1}])
    assert.throws(() => args(options), /partition/i, JSON.stringify(options));
});

test('native partition helpers and actual runner argument parser cover all supported counts', t => {
  const compiler = spawnSync(process.env.CC || 'cc', ['--version'], {encoding:'utf8'});
  if (compiler.error?.code === 'ENOENT' && !process.env.CC) {
    t.skip('cc is unavailable; the native parser/partition assertions were not run');
    return;
  }
  const source = read('runners/video-contact-sheet.c');
  const parser = source.slice(source.indexOf('#define PROGRESS_PREFIX'), source.indexOf('static const AVPacketSideData'));
  const directory = fs.mkdtempSync(path.join(os.tmpdir(), 'contact-partition-'));
  try {
    const harness = `#include <assert.h>\n#include <errno.h>\n#include <stdio.h>\n#include <stdlib.h>\n#include <string.h>\n#include "video-contact-sheet-partition.h"\n#define AVERROR(code) (-(code))\nstatic const char *av_version_info(void) { return "test"; }\n${parser}\n
static int parse_partition(const char *count, const char *index) {
  RunnerOptions options;
  char *argv[] = {"test", "--input", "input", "--output", "output", "--partition-count", (char *)count, "--partition-index", (char *)index};
  return parse_options(9, argv, &options);
}
int main(void) {
  RunnerOptions options;
  char *legacy[] = {"test", "--input", "input", "--output", "output"};
  assert(parse_options(5, legacy, &options) == 0);
  assert(options.partition_count == 1 && options.partition_index == 0);
  assert(parse_partition("2", "0") == 0 && parse_partition("2", "1") == 0);
  assert(parse_partition("1", "1") < 0 && parse_partition("3", "0") < 0);
  assert(parse_partition("2", "-1") < 0 && parse_partition("2", "2") < 0);
  assert(parse_partition("2", "") < 0 && parse_partition("", "0") < 0);
  assert(parse_partition("2.0", "0") < 0 && parse_partition("2147483648", "0") < 0);
  const int counts[] = {12,24,48};
  for (int c = 0; c < 3; c++) {
    int visits[48] = {0};
    for (int part = 0; part < 2; part++) {
      int owned = 0;
      for (int i = 0; i < counts[c]; i++) {
        if (contact_sheet_partition_owns(i, part, 2)) { visits[i]++; owned++; }
        assert(contact_sheet_partition_owns(i, 0, 1));
      }
      assert(owned == contact_sheet_partition_size(counts[c], part, 2));
    }
    for (int i = 0; i < counts[c]; i++) assert(visits[i] == 1);
  }
  return 0;
}`;
    fs.writeFileSync(path.join(directory, 'test.c'), harness);
    const executable = path.join(directory, process.platform === 'win32' ? 'test.exe' : 'test');
    const compile = spawnSync(process.env.CC || 'cc', ['-std=c11','-Wall','-Wextra','-Werror','-I',path.join(root,'runners'),path.join(directory,'test.c'),'-o',executable], {encoding:'utf8'});
    assert.equal(compile.status, 0, compile.error?.message || compile.stderr);
    const run = spawnSync(executable, [], {encoding:'utf8'});
    assert.equal(run.status, 0, run.stderr);
  } finally { fs.rmSync(directory, {recursive:true,force:true}); }
});

test('profile remains single-threaded and advertises the bounded partition capability', () => {
  const env = read('profiles/video-contact-sheet/profile.env');
  assert.match(env, /"partitionSampling":true,"maxPartitionCount":2/);
  assert.doesNotMatch(env, /multi-thread/);
  assert.match(read('runners/video-contact-sheet.c'), /decoder->thread_count = 1;/);
});

test('cancelled partition workers cannot emit late progress', async () => {
  let worker;
  class Worker { constructor(){worker=this;} postMessage(){} terminate(){this.terminated=true;} }
  const {api,revoked}=runtime(Worker);
  const runner=await api.loadEmbedded({coreJsText:'',wasmBytes:new Uint8Array([0])});
  const controller=new AbortController();let progress=0;
  const promise=runner.run({signal:controller.signal,onProgress:()=>progress++});
  controller.abort();await assert.rejects(promise,{name:'AbortError'});
  worker.onmessage({data:{type:'progress',progress:.5}});
  assert.equal(progress,0);assert.equal(worker.terminated,true);assert.equal(revoked.length,1);
  assert.equal(runner.activeRuns.size,0);
});

test('worker messaging failures release Worker, URL and active operation', {timeout:500}, async () => {
  for(const mode of ['post','message']) {
    let worker;
    class Worker { constructor(){worker=this;} postMessage(){if(mode==='post')throw new Error('clone failed');} terminate(){this.terminated=true;} }
    const {api,revoked}=runtime(Worker);
    const runner=await api.loadEmbedded({coreJsText:'',wasmBytes:new Uint8Array([0])});
    const promise=runner.run({});
    if(mode==='message')worker.onmessageerror?.({});
    await assert.rejects(promise,/clone|message|deserialize/i);
    assert.equal(worker.terminated,true);assert.equal(revoked.length,1);assert.equal(runner.activeRuns.size,0);
  }
});
