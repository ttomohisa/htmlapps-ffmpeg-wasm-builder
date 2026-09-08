const runner = await BrowserFFmpeg.loadEmbedded({ coreJsText, wasmBytes, threading: threadingMode });
if (threadingMode === "multi-thread") {
  if (!crossOriginIsolated) throw new Error("Multi-thread smoke test is not cross-origin isolated.");
  if (typeof SharedArrayBuffer !== "function") throw new Error("SharedArrayBuffer is unavailable in the multi-thread smoke test.");
}

const outputPath = "/output.mp4";
let lastProgress = 0;
const result = await runner.run({
  files: [{ name: "/workerfs/input.mp4", data: new File([input], "smoke-input.mp4", { type: "video/mp4" }), workerfs: true }],
  outputs: [outputPath],
  args: BrowserFFmpeg.ffmpegFilterBuilderArgs({
    input: "/workerfs/input.mp4",
    output: outputPath,
    videoFilter: "scale=160:90",
    startTimeSeconds: 0.25,
    durationSeconds: 0.5,
    maxWidth: 160,
    maxHeight: 90,
    crf: 34,
    noAudio: false
  }),
  onProgress: (value) => { lastProgress = value; append("progress=" + value.toFixed(3)); },
  onLog: ({ message }) => append(message)
});
if (result.exitCode !== 0) throw new Error("Runner exit code was " + result.exitCode);
if (!result.files || result.files.length !== 1) throw new Error("Expected one MP4 output");
const output = result.files[0].data;
if (output.byteLength < 1024) throw new Error("Filter Builder MP4 is unexpectedly small: " + output.byteLength);
if (String.fromCharCode(...output.slice(4, 8)) !== "ftyp") throw new Error("Filter Builder output is not MP4");
if (!containsAscii(output, "avc1")) throw new Error("H.264 marker missing from Filter Builder output");
if (!containsAscii(output, "mp4a")) throw new Error("AAC marker missing from Filter Builder output");
if (lastProgress < 0.99) throw new Error("Progress did not reach completion");

const inspectPath = "/inspect.json";
const inspect = await runner.run({
  files: [{ name: "/workerfs/trimmed.mp4", data: new File([output], "trimmed.mp4", { type: "video/mp4" }), workerfs: true }],
  outputs: [inspectPath],
  args: ["--input", "/workerfs/trimmed.mp4", "--inspect-output", inspectPath],
  onLog: ({ message }) => append(message)
});
const report = BrowserFFmpeg.decodeJsonOutput(inspect, inspectPath);
if (!(report.duration >= 0.30 && report.duration <= 0.75)) {
  throw new Error("Time-range render duration was unexpected: " + report.duration);
}
if (!report.audio || report.audio.codec !== "aac") throw new Error("Trimmed output did not preserve AAC audio");
runner.dispose();
pass("threading=" + threadingMode + ";bytes=" + output.byteLength + ";duration=" + report.duration.toFixed(3));
