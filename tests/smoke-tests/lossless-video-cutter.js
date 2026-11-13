// Inspect the actual MP4 tracks and sample counts, not just codec-name strings.
// The synthetic input has two AAC streams, and the old cutter dropped both
// the second stream and the final audio packets near a B-frame boundary.
function mp4StreamCounts(buffer, handler) {
  const bytes = buffer instanceof Uint8Array ? buffer : new Uint8Array(buffer);
  const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  const ascii = (offset) => String.fromCharCode(...bytes.subarray(offset, offset + 4));
  const boxes = (start, end) => {
    const found = [];
    while (start + 8 <= end) {
      const size = view.getUint32(start);
      if (size < 8 || start + size > end) throw new Error('Malformed MP4 atom');
      found.push({name: ascii(start + 4), start: start + 8, end: start + size});
      start += size;
    }
    return found;
  };
  const find = (list, name) => {
    const box = list.find(box => box.name === name);
    if (!box) throw new Error('Missing MP4 atom ' + name);
    return box;
  };
  const root = boxes(0, bytes.length);
  const moov = find(root, 'moov');
  return boxes(moov.start, moov.end).filter(box => box.name === 'trak').flatMap(track => {
    const mdia = find(boxes(track.start, track.end), 'mdia');
    const parts = boxes(mdia.start, mdia.end);
    const hdlr = find(parts, 'hdlr');
    if (ascii(hdlr.start + 8) !== handler) return [];
    const minf = find(parts, 'minf');
    const stbl = find(boxes(minf.start, minf.end), 'stbl');
    const stsz = find(boxes(stbl.start, stbl.end), 'stsz');
    return [view.getUint32(stsz.start + 8)];
  });
}
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
const inputAudio = mp4StreamCounts(input, 'soun');
const cutAudio = mp4StreamCounts(tailResult.files[0].data, 'soun');
if (inputAudio.length !== 2 || cutAudio.length !== 2)
  throw new Error('The cut must preserve both AAC tracks: ' + inputAudio + ' -> ' + cutAudio);
// 48 kHz AAC packets are 1024 samples (~21.33ms); for a cut ending
// at 0.52 seconds, all 25 packets whose presentation times are
// 0.000..0.512s must remain. The old video-triggered break kept only 21-22.
if (cutAudio.some(count => count < 25))
  throw new Error('Cut lost trailing AAC packets: sample counts=' + cutAudio.join(','));

const cutSubtitles = mp4StreamCounts(tailResult.files[0].data, 'sbtl');
if (mp4StreamCounts(input, 'sbtl').length !== 1 || cutSubtitles.length !== 1 ||
    cutSubtitles[0] < 1)
  throw new Error('Cut must preserve a supported mov_text subtitle track');

const noAudioResult = await BrowserFFmpeg.loadEmbedded({coreJsText, wasmBytes});
const noAudioCut = await noAudioResult.run({
  files: [{name: '/workerfs/muted.mp4', data: new Blob([input], {type: 'video/mp4'}), workerfs: true}],
  outputs: ['/muted.mp4'],
  args: BrowserFFmpeg.losslessVideoCutterArgs({
    input: '/workerfs/muted.mp4', output: '/muted.mp4', start: 0, end: boundary, noAudio: true
  })
});
noAudioResult.dispose();
if (noAudioCut.exitCode !== 0 || noAudioCut.files?.length !== 1 ||
    mp4StreamCounts(noAudioCut.files[0].data, 'soun').length !== 0 ||
    mp4StreamCounts(noAudioCut.files[0].data, 'sbtl').length !== 1)
  throw new Error('--no-audio must remove every audio track');

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
pass("bytes=" + output.byteLength + "_actualStart=" + actualStart.toFixed(6) + "_frames=" + cutVideo.sampleCount + "_audios=" + cutAudio.length + "_subtitles=" + cutSubtitles.length);
