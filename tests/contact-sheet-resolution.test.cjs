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

test('contact-sheet runtime accepts the bounded Ultra sizes and preserves the default', () => {
  for (const thumbSize of [96,320,640,660,840,960]) {
    const argv = args({thumbSize});
    assert.equal(argv[argv.indexOf('--thumb-size') + 1], String(thumbSize));
  }
  assert.deepEqual(args({}), ['--input','/workerfs/input.mp4','--output','/contact-sheet.ppm','--count','12','--thumb-size','320']);
});

test('contact-sheet runtime rejects malformed and out-of-budget thumbnail sizes', () => {
  for (const thumbSize of [0,95,961,1000000,96.5,NaN,Infinity,'bad'])
    assert.throws(() => args({thumbSize}), /thumbSize/, String(thumbSize));
});

test('actual C parser and geometry preserve legacy grids and bound Ultra allocations', t => {
  const compiler = spawnSync(process.env.CC || 'cc', ['--version'], {encoding:'utf8'});
  if (compiler.error?.code === 'ENOENT' && !process.env.CC) {
    t.skip('cc is unavailable; the native parser/geometry assertions were not run');
    return;
  }
  const source = read('runners/video-contact-sheet.c');
  const parser = source.slice(source.indexOf('#define PROGRESS_PREFIX'), source.indexOf('static const AVPacketSideData'));
  const geometry = source.slice(source.indexOf('static int compute_geometry'), source.indexOf('static int64_t video_duration_us'));
  const directory = fs.mkdtempSync(path.join(os.tmpdir(), 'contact-resolution-'));
  try {
    // Only FFmpeg metadata access is stubbed; parser and geometry are the actual
    // runner functions, including the allocation budget check being exercised.
    const harness = `#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "video-contact-sheet-partition.h"
#define AVERROR(code) (-(code))
static const char *av_version_info(void) { return "test"; }
${parser}
typedef struct { int width, height, rotation; } AVCodecParameters;
typedef struct { AVCodecParameters *codecpar; double sar; } AVStream;
static int normalized_quarter_rotation(const AVCodecParameters *par) { return par->rotation; }
static double effective_sample_aspect_ratio(const AVStream *stream) { return stream->sar; }
${geometry}
static int parse_size(const char *size) {
  RunnerOptions options;
  char *argv[] = {"test", "--input", "input", "--output", "output", "--thumb-size", (char *)size};
  return parse_options(7, argv, &options);
}
int main(void) {
  assert(contact_sheet_partition_owns(0, 0, 1));
  assert(contact_sheet_partition_size(12, 0, 1) == 12);
  RunnerOptions options;
  SheetGeometry grid;
  AVCodecParameters par = {1920,1080,0};
  AVStream stream = {&par,1.0};
  char *legacy[] = {"test", "--input", "input", "--output", "output"};
  assert(parse_options(5, legacy, &options) == 0);
  assert(options.thumb_size == 320 && options.count == 12 && options.columns == 4);
  assert(compute_geometry(&options, &stream, &grid) == 0);
  assert(grid.cell_width == 320 && grid.cell_height == 180);
  assert(grid.sheet_width == 1280 && grid.sheet_height == 540);
  assert(parse_size("640") == 0 && parse_size("660") == 0 && parse_size("840") == 0 && parse_size("960") == 0);
  const char *invalid[] = {"95","961","0","-1","960.0","","bad","2147483648"};
  for (unsigned i=0; i<sizeof(invalid)/sizeof(invalid[0]); i++) assert(parse_size(invalid[i]) < 0);
  const int counts[] = {12,24,48};
  const int sizes[] = {960,840,660};
  const int widths[] = {3840,5040,5280};
  const int heights[] = {1620,1892,2226};
  for (int i=0; i<3; i++) {
    options.count = counts[i]; options.columns = default_columns_for_count(counts[i]); options.thumb_size = sizes[i];
    assert(compute_geometry(&options, &stream, &grid) == 0);
    assert(grid.sheet_width == widths[i] && grid.sheet_height == heights[i]);
    par.rotation = 90;
    assert(compute_geometry(&options, &stream, &grid) == 0);
    assert(grid.cell_height == sizes[i] && grid.prescale_width == sizes[i]);
    assert(grid.sheet_width == options.columns * (int)llround(sizes[i]*9.0/16.0));
    par.rotation = 0;
  }
  // The square source is the worst case for every permitted size/grid.
  par.width = par.height = 1000;
  for (int i=0; i<3; i++) {
    options.count = counts[i]; options.thumb_size = 640;
    for (int columns=1; columns<=16 && columns<=options.count; columns++) {
      options.columns = columns;
      assert(compute_geometry(&options, &stream, &grid) == 0);
    }
    options.columns = default_columns_for_count(counts[i]); options.thumb_size = sizes[i];
    assert(compute_geometry(&options, &stream, &grid) == 0);
    assert((int64_t)grid.sheet_width * grid.sheet_height <= 25000000);
  }
  // 48 x 660 square cells fit; 48 x 960 square cells exceed the RGB budget.
  options.count = 48; options.columns = 8; options.thumb_size = 960;
  assert(compute_geometry(&options, &stream, &grid) < 0);
  // Partial last rows also count towards allocation, not just sample count.
  options.thumb_size = 660; options.columns = 15;
  assert(compute_geometry(&options, &stream, &grid) < 0);
  options.thumb_size = 640;
  assert(compute_geometry(&options, &stream, &grid) == 0);
  assert((int64_t)grid.sheet_width * grid.sheet_height == 24576000);
  par.width = 0;
  assert(compute_geometry(&options, &stream, &grid) < 0);
  return 0;
}`;
    fs.writeFileSync(path.join(directory, 'test.c'), harness);
    const executable = path.join(directory, process.platform === 'win32' ? 'test.exe' : 'test');
    const compile = spawnSync(process.env.CC || 'cc', ['-std=c11','-Wall','-Wextra','-Werror','-I',path.join(root,'runners'),path.join(directory,'test.c'),'-lm','-o',executable], {encoding:'utf8'});
    assert.equal(compile.status, 0, compile.error?.message || compile.stderr);
    const run = spawnSync(executable, [], {encoding:'utf8'});
    assert.equal(run.status, 0, run.stderr);
  } finally { fs.rmSync(directory, {recursive:true,force:true}); }
});

test('profile advertises the same thumbnail and sheet limits enforced by the runner', () => {
  const env = read('profiles/video-contact-sheet/profile.env');
  const capabilities = JSON.parse(env.match(/PROFILE_CAPABILITIES_JSON='([^']+)'/)[1]);
  assert.equal(capabilities.minThumbSize,96);
  assert.equal(capabilities.maxThumbSize,960);
  assert.equal(capabilities.maxSheetPixels,25000000);
});
