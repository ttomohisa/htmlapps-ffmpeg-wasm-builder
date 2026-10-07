// Supplemental actual ST core/MEMFS regression run. Browser ST/MT smoke remains required.
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const vm = require('node:vm');
const {execFileSync} = require('node:child_process');
const assert = require('node:assert/strict');
globalThis.TimingReaders = require('./timing-readers.js');
const timing = require('./filter-builder-timing-regressions.js');
const [coreDirectory, evidenceDirectory] = process.argv.slice(2);
assert.ok(coreDirectory, 'Provide the ST directory containing ffmpeg.js and ffmpeg.wasm');
for (const program of ['ffprobe', 'ffmpeg']) execFileSync(program, ['-version'], {stdio: 'ignore'});
const directory = evidenceDirectory ? path.resolve(evidenceDirectory) : fs.mkdtempSync(path.join(os.tmpdir(), 'filter-timing-'));
fs.mkdirSync(directory, {recursive: true});
const js = fs.readFileSync(path.join(coreDirectory, 'ffmpeg.js'), 'utf8');
const wasm = fs.readFileSync(path.join(coreDirectory, 'ffmpeg.wasm'));
const context = {window: {}, self: {location: {href: 'file:///ffmpeg.js'}}, console,
  URL, WebAssembly, TextDecoder, TextEncoder, performance, setTimeout, clearTimeout, Uint8Array, ArrayBuffer};
vm.createContext(context);
vm.runInContext(fs.readFileSync(path.join(__dirname, '../runtime/browser-ffmpeg.js'), 'utf8'), context);
vm.runInContext(js, context);
const fixtures = Object.fromEntries([['filterCfr', 'filter-cfr'], ['vfrTail', 'filter-vfr-tail'], ['vfr', 'vfr'], ['single', 'single']]
  .map(([name, file]) => [name, fs.readFileSync(path.join(__dirname, `fixtures/timing-${file}.mp4`))]));
const moduleBytes = new WebAssembly.Module(wasm);
const records = [];
async function run(files, args, output, expected) {
  const logs = [];
  const core = await context.createFFmpegCore({wasmBinary: wasm,
    instantiateWasm: (imports, success) => {
      const instance = new WebAssembly.Instance(moduleBytes, imports);
      success(instance, moduleBytes); return instance.exports;
    }, print: () => {}, printErr: value => logs.push(String(value))});
  core.FS.mkdirTree('/workerfs');
  for (const file of files) core.FS.writeFile(file.name, file.data);
  let exitCode;
  try { exitCode = core.callMain(args); }
  catch (error) { if (typeof error.status !== 'number') throw error; exitCode = error.status; }
  const data = core.FS.analyzePath(output).exists ? Uint8Array.from(core.FS.readFile(output)) : new Uint8Array();
  fs.writeFileSync(path.join(directory, expected.name + '.mp4'), data);
  fs.writeFileSync(path.join(directory, expected.name + '.log'), logs.join('\n'));
  records.push({name: expected.name, args, exitCode, bytes: data.length});
  if (exitCode) console.log(logs.slice(-5).join('\n'));
  return {exitCode, data, logs};
}
function pixels(filename, seconds) {
  const args = ['-v', 'error', '-i', filename];
  if (seconds) args.push('-ss', String(seconds));
  return timing.pixelSummary(execFileSync('ffmpeg', [...args, '-frames:v', '1', '-vf', 'hue=s=0,scale=32:18:flags=bilinear',
    '-f', 'rawvideo', '-pix_fmt', 'rgb24', '-']), 3);
}
const reference = pixels(path.join(__dirname, 'fixtures/timing-filter-cfr.mp4'), 1).gray;
const referenceControlRejected = timing.rejectWrongStart(pixels(path.join(__dirname, 'fixtures/timing-filter-cfr.mp4'), 0).gray, reference);
async function decode(bytes, expected) {
  const filename = path.join(directory, expected.name + '.mp4');
  const probe = JSON.parse(execFileSync('ffprobe', ['-v', 'error', '-count_frames', '-show_streams', '-of', 'json', filename], {encoding: 'utf8'}));
  fs.writeFileSync(path.join(directory, expected.name + '.ffprobe.json'), JSON.stringify(probe, null, 2) + '\n');
  const video = probe.streams.find(stream => stream.codec_type === 'video');
  assert.ok(video, 'normal demux/decode found no video stream');
  // Do not grayscale the output before measuring: that would hide color errors.
  const rgb = execFileSync('ffmpeg', ['-v', 'error', '-i', filename, '-frames:v', '1', '-vf', 'scale=32:18:flags=bilinear',
    '-f', 'rawvideo', '-pix_fmt', 'rgb24', '-']);
  const summary = timing.pixelSummary(rgb, 3);
  const result = {codec: video.codec_name, count: Number(video.nb_read_frames), width: video.width, height: video.height,
    yuv420p: video.pix_fmt === 'yuv420p', ...summary, referenceControlRejected, referenceError: expected.sourceStart === 1 ? timing.referenceError(summary.gray, reference) : undefined};
  const audio = probe.streams.find(stream => stream.codec_type === 'audio');
  if (audio) {
    const pcmBytes = execFileSync('ffmpeg', ['-v', 'error', '-i', filename, '-map', '0:a:0', '-ac', '1', '-f', 'f32le', '-acodec', 'pcm_f32le', '-'], {maxBuffer: 8 * 1024 * 1024});
    const pcm = new Float32Array(Uint8Array.from(pcmBytes).buffer);
    result.audio = {codec: audio.codec_name, channels: audio.channels, sampleRate: Number(audio.sample_rate),
      duration: Number(audio.duration), toneHz: timing.toneFrequency(pcm, Number(audio.sample_rate))};
  }
  return result;
}
timing.run(fixtures, run, context.window.BrowserFFmpeg, decode, console.log)
  .then(count => console.log(`FILTER_TIMING_PASS ST/MEMFS: ${count} cases`))
  .catch(error => { console.error(error.stack); process.exitCode = 1; })
  .finally(() => {
    if (evidenceDirectory) fs.writeFileSync(path.join(directory, 'execution.json'), JSON.stringify(records, null, 2) + '\n');
    else fs.rmSync(directory, {recursive: true, force: true});
  });
