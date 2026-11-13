const messages = [];
const runner = await BrowserFFmpeg.loadEmbedded({ coreJsText, wasmBytes });
const result = await runner.run({
  files: [{ name: "/workerfs/input.mp4", data: new Blob([input], { type: "video/mp4" }), workerfs: true }],
  outputs: ["/output.mp4"],
  args: BrowserFFmpeg.losslessVideoCutterArgs({
    input: "/workerfs/input.mp4",
    output: "/output.mp4",
    start: 0.20,
    end: 0.72
  }),
  onLog: ({ message }) => { messages.push(String(message)); append(message); }
});
// Keep the runtime alive for an independent end-of-range packet regression.
const sourceVideo = TimingReaders.readMp4(input).video;
if (!sourceVideo.pts.some((pts, index) =>
  index > 0 && pts < sourceVideo.pts[index - 1])) {
  throw new Error("The cutter smoke fixture must contain reordered B frames");
}
const boundary = 0.52;
const tailResult = await runner.run({
  files: [{ name: "/workerfs/boundary.mp4", data: new Blob([input], { type: "video/mp4" }), workerfs: true }],
  outputs: ["/boundary.mp4"],
  args: BrowserFFmpeg.losslessVideoCutterArgs({
    input: "/workerfs/boundary.mp4", output: "/boundary.mp4", start: 0, end: boundary
  })
});
runner.dispose();
if (tailResult.exitCode !== 0 || tailResult.files?.length !== 1)
  throw new Error("Reordered-frame boundary cut failed");
const cutVideo = TimingReaders.readMp4(tailResult.files[0].data).video;
const inputPts = sourceVideo.pts.map(value => value / sourceVideo.timescale);
const inputDts = sourceVideo.dts.map(value => value / sourceVideo.timescale);
const outputPts = cutVideo.pts.map(value => value / cutVideo.timescale);
const offset = Math.min(...outputPts) - Math.min(...inputPts);
// A reference P picture can be shown beyond the end but decoded BEFORE
// trailing B pictures. Checking only pre-end PTS would miss this corruption.
if (!inputPts.some((value, index) => value >= boundary && inputDts[index] < boundary))
  throw new Error("The B-frame fixture lacks a post-end reference packet");
for (let index = 0; index < inputPts.length; index++) {
  const sourcePts = inputPts[index];
  if (sourcePts >= boundary - 0.0001 && inputDts[index] >= boundary - 0.0001)
    continue;
  if (!outputPts.some(value => Math.abs(value - offset - sourcePts) < .002)) {
    throw new Error("Cut dropped a pre-end B frame or its reference at " + sourcePts.toFixed(3) + "s");
  }
}


if (result.exitCode !== 0) throw new Error("Runner exit code was " + result.exitCode);
if (!result.files || result.files.length !== 1) throw new Error("Expected exactly one output file");
const output = result.files[0].data;
if (!(output instanceof Uint8Array)) throw new Error("Output is not a Uint8Array");
if (output.byteLength < 1024) throw new Error("Cut output is unexpectedly small: " + output.byteLength);
if (output.byteLength >= input.byteLength) throw new Error("Cut output should be smaller than the 1-second smoke input");
if (String.fromCharCode(...output.slice(4, 8)) !== "ftyp") throw new Error("Output does not contain an MP4 ftyp box at offset 4");
if (!containsAscii(output, "moov")) throw new Error("Cut output does not contain a moov box");
if (!containsAscii(output, "mdat")) throw new Error("Cut output does not contain an mdat box");
if (!containsAscii(output, "avc1")) throw new Error("Cut output lost the H.264/avc1 video stream");
if (!containsAscii(output, "mp4a")) throw new Error("Cut output lost the AAC/mp4a audio stream");
const startLine = messages.find((line) =>
  line.includes("lossless-cut: requested-start=") &&
  line.includes(" actual-start=") &&
  line.includes(" keyframe-aligned=yes")
);
if (!startLine) throw new Error("Runner did not report the actual keyframe-aligned start");
const actualStartMatch = /(?:^|\s)actual-start=([0-9]+(?:\.[0-9]+)?)/.exec(startLine);
if (!actualStartMatch) throw new Error("Runner reported an unreadable actual-start value: " + startLine);
const actualStart = Number(actualStartMatch[1]);
if (!Number.isFinite(actualStart) || actualStart < 0 || actualStart > 0.201) {
  throw new Error("Keyframe-aligned start must not move after the requested 0.20 s start: " + startLine);
}
pass("bytes=" + output.byteLength + "_actualStart=" + actualStart.toFixed(6) + "_frames=" + cutVideo.sampleCount);
