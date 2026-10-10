const runner = await BrowserFFmpeg.loadEmbedded({ coreJsText, wasmBytes, threading: threadingMode });
const threadLogs = [];
const recordLog = ({message}) => { threadLogs.push(message); append(message); };
const assertThreading = codec => {
  const threads = threadingMode === "multi-thread" ? 4 : 1;
  const rowMt = threadingMode === "multi-thread" && codec === "libvpx-vp9" ? 1 : 0;
  if (!threadLogs.some(line => line.includes(`opening ${codec} encoder: threads=${threads} row_mt=${rowMt}`)))
    throw new Error(`Incorrect ${codec} threading: ${threadingMode}`);
};
const inputFile = new File([input], "smoke-rotated.mp4", { type: "video/mp4" });
const inputPath = "/workerfs/input.mp4";

const inspect = async (file, path, output) => {
  const result = await runner.run({
    files: [{ name: path, data: file, workerfs: true }],
    outputs: [output],
    args: BrowserFFmpeg.videoCompressorInspectArgs({ input: path, output }),
    onLog: recordLog
  });
  return BrowserFFmpeg.decodeJsonOutput(result, output);
};

const inputInfo = await inspect(inputFile, inputPath, "/probe.json");
append("inspect=" + JSON.stringify(inputInfo));
if (!(inputInfo?.video?.bitRateKbps > 0)) throw new Error("Measured input video bitrate is missing");
if (inputInfo.video.width !== 160 || inputInfo.video.height !== 96) throw new Error("Unexpected coded input dimensions");
if (inputInfo.video.displayWidth !== 96 || inputInfo.video.displayHeight !== 160) throw new Error("Display-matrix dimensions were not detected");
if (Math.abs(Number(inputInfo.video.rotation) - 90) > 1) throw new Error("90-degree input rotation was not detected");

const h264 = await runner.run({
  files: [{ name: inputPath, data: inputFile, workerfs: true }],
  outputs: ["/output.mp4"],
  args: BrowserFFmpeg.videoCompressorArgs({
    input: inputPath, output: "/output.mp4", codec: "h264", speed: "fastest",
    maxWidth: 96, videoBitrateKbps: Math.max(100, inputInfo.video.bitRateKbps), audioBitrateKbps: 32
  }),
  onLog: recordLog
});
if (h264.exitCode !== 0 || h264.files?.length !== 1) throw new Error("H.264 encode failed");
assertThreading('libx264');
const mp4 = h264.files[0].data;
if (mp4.byteLength < 1024 || String.fromCharCode(...mp4.slice(4, 8)) !== "ftyp") throw new Error("H.264 MP4 output is invalid");
if (!containsAscii(mp4, "avc1") || !containsAscii(mp4, "mp4a")) throw new Error("H.264/AAC markers are missing");
const audioVideoTiming = TimingReaders.readMp4(mp4).video;
if (audioVideoTiming.sampleCount !== 48 || audioVideoTiming.durations.some(duration => duration <= 0)) throw new Error("Audio/video MP4 lost a complete video sample");
if (Math.abs((audioVideoTiming.presentationEnd - audioVideoTiming.presentationStart) / audioVideoTiming.timescale - 2) > 1 / audioVideoTiming.timescale) throw new Error("Audio must not conceal a shortened video endpoint");
const h264Info = await inspect(new File([mp4], "output.mp4", { type: "video/mp4" }), "/workerfs/h264.mp4", "/h264.json");
if (h264Info.video.width !== 96 || h264Info.video.height !== 160) throw new Error("H.264 autorotation did not produce portrait pixels");
if (Math.abs(Number(h264Info.video.rotation || 0)) > 1) throw new Error("H.264 output should not depend on a rotation matrix");

const vp9 = await runner.run({
  files: [{ name: inputPath, data: inputFile, workerfs: true }],
  outputs: ["/output.webm"],
  args: BrowserFFmpeg.videoCompressorArgs({
    input: inputPath, output: "/output.webm", codec: "vp9", speed: "fastest",
    maxWidth: 96, videoBitrateKbps: Math.max(80, Math.round(inputInfo.video.bitRateKbps * 0.7)), audioBitrateKbps: 32
  }),
  onLog: recordLog
});
if (vp9.exitCode !== 0 || vp9.files?.length !== 1) throw new Error("VP9 encode failed");
assertThreading('libvpx-vp9');
const webm = vp9.files[0].data;
if (webm.byteLength < 1024) throw new Error("VP9 WebM output is unexpectedly small");
if (!(webm[0] === 0x1a && webm[1] === 0x45 && webm[2] === 0xdf && webm[3] === 0xa3)) throw new Error("Output does not start with an EBML header");
if (!containsAscii(webm, "V_VP9")) throw new Error("VP9 codec marker is missing");
if (!containsAscii(webm, "A_OPUS")) throw new Error("Opus codec marker is missing");
const vp9Info = await inspect(new File([webm], "output.webm", { type: "video/webm" }), "/workerfs/vp9.webm", "/vp9.json");
if (vp9Info.video.width !== 96 || vp9Info.video.height !== 160) throw new Error("VP9 autorotation did not produce portrait pixels");
if (Math.abs(Number(vp9Info.video.rotation || 0)) > 1) throw new Error("VP9 output should not depend on a rotation matrix");

await runTimingRegressions("video-compressor", timingFixtures, async (bytes, args, output) => {
  let result;
  try { result = await runner.run({
    files: [{name: "/workerfs/timing-input.mp4", data: new Blob([bytes]), workerfs: true}],
    outputs: [output], args,
    onLog: ({message}) => append(message)
  });
  } catch (error) {
    const match = /FFmpeg WASM runner exited with code (\d+)/.exec(error.message);
    if (match) return {exitCode: Number(match[1])};
    throw error;
  }
  return {exitCode: result.exitCode, data: result.files?.find(file => file.name === output)?.data};
}, BrowserFFmpeg, append);
runner.dispose();

pass("h264=" + mp4.byteLength + "_vp9=" + webm.byteLength + "_bitrate=" + inputInfo.video.bitRateKbps + "_rotation=" + inputInfo.video.rotation);
