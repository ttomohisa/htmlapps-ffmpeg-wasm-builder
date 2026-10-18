// Test-only observation wraps the embedded factory; release ffmpeg.js is unchanged.
// Only the outer runner passes instantiateWasm; nested Emscripten workers do not
// send our diagnostic messages into their internal pthread protocol.
const observation = String.raw`
;{
  const originalFactory = createFFmpegCore;
  createFFmpegCore = async options => {
    if (typeof options?.instantiateWasm !== 'function') return originalFactory(options);
    const instantiate = options.instantiateWasm;
    const OriginalWorker = self.Worker;
    const log = message => self.postMessage({type:'log', stream:'stdout', message});
    const observed = {...options, instantiateWasm(imports, success) {
      return instantiate(imports, (instance, module) => {
        const values = [...Object.values(instance.exports), ...Object.values(imports).flatMap(group => Object.values(group))];
        const memory = values.find(value => value instanceof WebAssembly.Memory);
        if (!memory) throw new Error('Smoke test could not observe the actual WASM memory.');
        const shared = typeof SharedArrayBuffer === 'function' && memory.buffer instanceof SharedArrayBuffer;
        log('__SPEED_MEMORY_SHARED__=' + shared);
        success(instance, module);
      });
    }};
    if (options.mainScriptUrlOrBlob) self.Worker = class extends OriginalWorker {
      constructor(...args) { super(...args); log('__SPEED_PTHREAD_CREATED__'); }
    };
    try { return await originalFactory(observed); }
    finally { self.Worker = OriginalWorker; }
  };
}
`;
const observedCoreJsText = coreJsText + observation;
const runner = await BrowserFFmpeg.loadEmbedded({ coreJsText: observedCoreJsText, wasmBytes, threading: threadingMode });
const threadLogs = [];
const recordLog = ({message}) => { threadLogs.push(message); append(message); };
const assertRuntimeObserved = () => {
  const expectedShared = threadingMode === 'multi-thread';
  if (!threadLogs.includes('__SPEED_MEMORY_SHARED__=' + expectedShared))
    throw new Error('Actual WASM memory does not match the selected threading variant.');
  const workers = threadLogs.filter(line => line === '__SPEED_PTHREAD_CREATED__').length;
  if (expectedShared ? workers !== 8 : workers !== 0)
    throw new Error(`Unexpected actual pthread Worker count: ${workers}`);
};
const assertThreading = () => {
  const decoderThreads = threadingMode === 'multi-thread' ? 2 : 1;
  const encoderThreads = threadingMode === 'multi-thread' ? 4 : 1;
  const lookaheadThreads = threadingMode === 'multi-thread' ? 1 : 0;
  if (!threadLogs.some(line => line.includes(`opening h264 decoder: threads=${decoderThreads}`)))
    throw new Error('Incorrect video decoder threading: ' + threadingMode);
  if (!threadLogs.some(line => line.includes(`opening libx264 encoder: threads=${encoderThreads} x264_lookahead_threads=${lookaheadThreads}`)))
    throw new Error('Incorrect video encoder threading: ' + threadingMode);
};
const inputFile = new File([input], "smoke-input.mp4", { type: "video/mp4" });

const durationOf = async (bytes) => {
  const url = URL.createObjectURL(new Blob([bytes], { type: "video/mp4" }));
  try {
    const video = document.createElement("video");
    video.preload = "metadata";
    return await new Promise((resolve, reject) => {
      video.onloadedmetadata = () => resolve(video.duration);
      video.onerror = () => reject(new Error("Browser could not read output metadata"));
      video.src = url;
    });
  } finally {
    URL.revokeObjectURL(url);
  }
};

const assertDuration = (sourceDuration, rate, duration, label) => {
  const expected = sourceDuration / rate;
  const tolerance = Math.max(0.12, expected * 0.18);
  if (Math.abs(duration - expected) > tolerance) {
    throw new Error(`Unexpected ${label} duration at ${rate}x: ${duration} (expected about ${expected})`);
  }
};

const runRate = async (rate, options = {}) => {
  const preservePitch = options.preservePitch !== false;
  const noAudio = options.noAudio === true;
  const sourceFile = options.inputFile || inputFile;
  const sourceDuration = Number(options.sourceDuration ?? 1);
  const expectAudio = options.expectAudio ?? !noAudio;
  const mode = noAudio ? "drop-audio" : (preservePitch ? "preserve-pitch" : "shift-pitch");
  const label = `${rate}x/${mode}${options.labelSuffix ? "/" + options.labelSuffix : ""}`;
  let lastProgress = 0;
  append("case=" + label + " start");
  let result;
  threadLogs.length = 0;
  try {
    result = await runner.run({
      files: [{ name: "/workerfs/input.mp4", data: sourceFile, workerfs: true }],
      outputs: ["/output.mp4"],
      args: BrowserFFmpeg.videoSpeedChangerArgs({
        input: "/workerfs/input.mp4",
        output: "/output.mp4",
        rate,
        encoderSpeed: "fastest",
        crf: 30,
        audioBitrateKbps: 32,
        preservePitch,
        noAudio
      }),
      onProgress: (value) => {
        lastProgress = value;
        append("case=" + label + " progress=" + value.toFixed(3));
      },
      onLog: entry => {
        recordLog(entry);
        append("case=" + label + " " + entry.stream + ": " + entry.message);
      }
    });
  } catch (error) {
    throw new Error("Speed change failed at " + label + ": " + (error?.message || error), { cause: error });
  }

  if (result.exitCode !== 0 || result.files?.length !== 1) throw new Error("Speed change failed at " + label);
  assertRuntimeObserved();
  assertThreading();
  const output = result.files[0].data;
  if (output.byteLength < 1024 || String.fromCharCode(...output.slice(4, 8)) !== "ftyp") throw new Error("Invalid MP4 at " + label);
  if (!containsAscii(output, "avc1")) throw new Error("H.264 marker missing at " + label);
  if (!expectAudio) {
    if (containsAscii(output, "mp4a")) throw new Error("Audio unexpectedly present at " + label);
  } else if (!containsAscii(output, "mp4a")) {
    throw new Error("AAC marker missing at " + label);
  }
  if (lastProgress < 0.99) throw new Error("Progress did not reach completion at " + label);

  const duration = await durationOf(output);
  assertDuration(sourceDuration, rate, duration, label);
  append("case=" + label + " duration=" + duration.toFixed(3));
  return { bytes: output, duration, label };
};

append("case=inspect start");
const inspectResult = await runner.run({
  files: [{ name: "/workerfs/input.mp4", data: inputFile, workerfs: true }],
  outputs: ["/inspect.json"],
  args: BrowserFFmpeg.videoSpeedChangerInspectArgs({ input: "/workerfs/input.mp4", output: "/inspect.json" }),
  onLog: recordLog
});
assertRuntimeObserved();
const inspectReport = BrowserFFmpeg.decodeJsonOutput(inspectResult, "/inspect.json");
if (!inspectReport?.video?.width || !inspectReport?.video?.height || !inspectReport?.audio) throw new Error("Video Speed Changer inspect helper returned incomplete media information.");
append("case=inspect ok");

append("case=abort start");
const abortController = new AbortController();
threadLogs.length = 0;
const abortedRun = runner.run({
  files: [{ name: "/workerfs/input.mp4", data: inputFile, workerfs: true }],
  outputs: ["/output.mp4"],
  args: BrowserFFmpeg.videoSpeedChangerArgs({ input: "/workerfs/input.mp4", output: "/output.mp4", rate: 0.25, encoderSpeed: "fastest", crf: 30, audioBitrateKbps: 32 }),
  signal: abortController.signal,
  onLog: entry => {
    recordLog(entry);
    // The real core and pthread pool are running before cancellation.
    if (entry.message.includes('opening libx264 encoder:')) abortController.abort();
  }
});
let abortName = "";
try { await abortedRun; } catch (error) { abortName = error?.name || ""; }
if (abortName !== "AbortError" || runner.activeRuns.size !== 0) throw new Error("AbortSignal did not cancel the active FFmpeg Worker.");
assertRuntimeObserved();
append("case=abort ok");
await runRate(1.5, { labelSuffix: "after-cancel" });

const rates = [0.25, 0.5, 0.75, 1.0, 1.25, 1.5, 2.0, 3.0, 4.0];
const preserveResults = [];
for (const rate of rates) preserveResults.push(await runRate(rate));
const shiftedResults = [];
for (const rate of [0.25, 1.5, 4.0]) shiftedResults.push(await runRate(rate, { preservePitch: false }));
const silentResults = [];
for (const rate of [0.25, 2.0, 4.0]) silentResults.push(await runRate(rate, { noAudio: true }));
const silent = silentResults[1];
const silentSource = new File([silent.bytes], "silent-input.mp4", { type: "video/mp4" });
const silentSourceResult = await runRate(1.25, {
  inputFile: silentSource,
  sourceDuration: silent.duration,
  expectAudio: false,
  labelSuffix: "source-no-audio"
});

runner.dispose();
pass(
  "threading=" + threadingMode + ";inspect=ok;abort=ok;retry=ok;rates=" + preserveResults.map(item => item.duration.toFixed(3)).join(",") +
  ";shift=" + shiftedResults.map(item => item.duration.toFixed(3)).join(",") +
  ";silent=" + silentResults.map(item => item.duration.toFixed(3)).join(",") +
  ";silent-source=" + silentSourceResult.duration.toFixed(3)
);
