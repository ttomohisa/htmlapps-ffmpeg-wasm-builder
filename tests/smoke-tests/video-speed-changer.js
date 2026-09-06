const runner = await BrowserFFmpeg.loadEmbedded({ coreJsText, wasmBytes });
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
      onLog: ({ stream, message }) => append("case=" + label + " " + stream + ": " + message)
    });
  } catch (error) {
    throw new Error("Speed change failed at " + label + ": " + (error?.message || error), { cause: error });
  }

  if (result.exitCode !== 0 || result.files?.length !== 1) throw new Error("Speed change failed at " + label);
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
  args: BrowserFFmpeg.videoSpeedChangerInspectArgs({ input: "/workerfs/input.mp4", output: "/inspect.json" })
});
const inspectReport = BrowserFFmpeg.decodeJsonOutput(inspectResult, "/inspect.json");
if (!inspectReport?.video?.width || !inspectReport?.video?.height || !inspectReport?.audio) throw new Error("Video Speed Changer inspect helper returned incomplete media information.");
append("case=inspect ok");

append("case=abort start");
const abortController = new AbortController();
const abortedRun = runner.run({
  files: [{ name: "/workerfs/input.mp4", data: inputFile, workerfs: true }],
  outputs: ["/output.mp4"],
  args: BrowserFFmpeg.videoSpeedChangerArgs({ input: "/workerfs/input.mp4", output: "/output.mp4", rate: 0.25, encoderSpeed: "fastest", crf: 30, audioBitrateKbps: 32 }),
  signal: abortController.signal
});
abortController.abort();
let abortName = "";
try { await abortedRun; } catch (error) { abortName = error?.name || ""; }
if (abortName !== "AbortError") throw new Error("AbortSignal did not cancel the active FFmpeg Worker.");
append("case=abort ok");

const rates = [0.25, 0.5, 0.75, 1.0, 1.25, 1.5, 2.0, 3.0, 4.0];
const preserveResults = [];
for (const rate of rates) preserveResults.push(await runRate(rate));
const shifted = await runRate(1.5, { preservePitch: false });
const silent = await runRate(2.0, { noAudio: true });
const silentSource = new File([silent.bytes], "silent-input.mp4", { type: "video/mp4" });
const silentSourceResult = await runRate(1.25, {
  inputFile: silentSource,
  sourceDuration: silent.duration,
  expectAudio: false,
  labelSuffix: "source-no-audio"
});

runner.dispose();
pass(
  "inspect=ok;abort=ok;rates=" + preserveResults.map(item => item.duration.toFixed(3)).join(",") +
  ";shift=" + shifted.duration.toFixed(3) +
  ";silent=" + silent.duration.toFixed(3) +
  ";silent-source=" + silentSourceResult.duration.toFixed(3)
);
