// Runs the packaged core's public runner against synthetic fixtures in MEMFS.
// This supplements, and does not replace, the real browser/WORKERFS smoke test.
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const assert = require('node:assert/strict');
globalThis.TimingReaders = require('./timing-readers.js');
const regressions = require('./timing-regressions.js');
const [profile, coreDirectory] = process.argv.slice(2);
assert.ok(['video-compressor', 'video-to-gif', 'video-to-webp'].includes(profile), 'Provide a timing profile and core directory');
assert.ok(coreDirectory, 'Provide the directory containing ffmpeg.js and ffmpeg.wasm');
const js = fs.readFileSync(path.join(coreDirectory, 'ffmpeg.js'), 'utf8');
const wasm = fs.readFileSync(path.join(coreDirectory, 'ffmpeg.wasm'));
const context = {window: {}, self: {location: {href: 'file:///ffmpeg.js'}}, console,
  URL, WebAssembly, TextDecoder, TextEncoder, performance, setTimeout, clearTimeout, Uint8Array, ArrayBuffer};
vm.createContext(context);
vm.runInContext(fs.readFileSync(path.join(__dirname, '../runtime/browser-ffmpeg.js'), 'utf8'), context);
vm.runInContext(js, context);
const fixtures = Object.fromEntries(['cfr', 'vfr', 'single'].map(name => [name,
  fs.readFileSync(path.join(__dirname, `fixtures/timing-${name}.mp4`))]));
async function run(input, args, output) {
  const logs = [];
  const core = await context.createFFmpegCore({wasmBinary: wasm,
    instantiateWasm: (imports, success) => {
      const module = new WebAssembly.Module(wasm);
      const instance = new WebAssembly.Instance(module, imports);
      success(instance, module); return instance.exports;
    }, print: () => {}, printErr: value => logs.push(String(value))});
  core.FS.mkdirTree('/workerfs');
  core.FS.writeFile('/workerfs/timing-input.mp4', input);
  let exitCode;
  try { exitCode = core.callMain(args); }
  catch (error) { if (typeof error.status !== 'number') throw error; exitCode = error.status; }
  let data = new Uint8Array();
  if (core.FS.analyzePath(output).exists) data = Uint8Array.from(core.FS.readFile(output));
  if (exitCode) console.log(logs.slice(-4).join('\n'));
  return {exitCode, data};
}
regressions(profile, fixtures, run, context.window.BrowserFFmpeg, console.log)
  .then(count => console.log(`TIMING_PASS ${profile}: ${count} cases`))
  .catch(error => { console.error(error.stack); process.exitCode = 1; });
