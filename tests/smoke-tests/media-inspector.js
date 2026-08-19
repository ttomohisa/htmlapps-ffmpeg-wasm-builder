const runner = await BrowserFFmpeg.loadEmbedded({ coreJsText, wasmBytes });
const reportPath = "/report.json";
const result = await runner.run({
  files: [{ name: "/workerfs/input.mp4", data: new Blob([input], { type: "video/mp4" }), workerfs: true }],
  outputs: [reportPath],
  args: BrowserFFmpeg.mediaInspectorArgs({
    input: "/workerfs/input.mp4",
    output: reportPath
  }),
  onLog: ({ message }) => append(message)
});
runner.dispose();

if (result.exitCode !== 0) throw new Error("Runner exit code was " + result.exitCode);
if (!result.files || result.files.length !== 1) throw new Error("Expected exactly one JSON output file");
const report = BrowserFFmpeg.decodeJsonOutput(result, reportPath);

if (report.schemaVersion !== 1) throw new Error("Unexpected report schema: " + report.schemaVersion);
if (report.runnerVersion !== "1.4.0") throw new Error("Unexpected runner version: " + report.runnerVersion);
if (!report.format?.name?.includes("mov")) throw new Error("MP4/MOV demuxer was not detected: " + report.format?.name);
if (report.format.fileSize !== input.byteLength) throw new Error("File size mismatch: " + report.format.fileSize + " vs " + input.byteLength);
if (!(report.format.duration > 0.9 && report.format.duration < 1.1)) throw new Error("Unexpected duration: " + report.format.duration);
if (!(report.format.bitRate > 0)) throw new Error("Total bitrate was not reported");
if (report.format.streamCount !== 2) throw new Error("Expected two streams, got " + report.format.streamCount);
if (!Array.isArray(report.streams) || report.streams.length !== 2) throw new Error("Stream report is incomplete");
if (!Array.isArray(report.chapters)) throw new Error("Chapter list is missing");

const video = report.streams.find((stream) => stream.type === "video");
if (!video) throw new Error("Video stream was not reported");
if (video.codec?.name !== "h264") throw new Error("Expected H.264, got " + video.codec?.name);
if (video.codec?.tag !== "avc1") throw new Error("Expected avc1 codec tag, got " + video.codec?.tag);
if (video.video?.width !== 160 || video.video?.height !== 90) throw new Error("Unexpected video dimensions");
if (!(video.video?.frameRate?.value > 11.9 && video.video?.frameRate?.value < 12.1)) throw new Error("Unexpected FPS: " + video.video?.frameRate?.value);
if (video.video?.hdr?.isHdr !== false) throw new Error("Smoke fixture should not be classified as HDR");

const audio = report.streams.find((stream) => stream.type === "audio");
if (!audio) throw new Error("Audio stream was not reported");
if (audio.codec?.name !== "aac") throw new Error("Expected AAC, got " + audio.codec?.name);
if (audio.audio?.sampleRate !== 48000) throw new Error("Unexpected audio sample rate: " + audio.audio?.sampleRate);
if (audio.audio?.channels !== 2) throw new Error("Unexpected channel count: " + audio.audio?.channels);
if (!String(audio.audio?.channelLayout || "").toLowerCase().includes("stereo")) throw new Error("Stereo layout was not reported: " + JSON.stringify(audio.audio));
if (typeof audio.audio?.channelLayoutInferred !== "boolean") throw new Error("channelLayoutInferred flag is missing");

pass("streams=" + report.streams.length + "_bytes=" + report.format.fileSize);
