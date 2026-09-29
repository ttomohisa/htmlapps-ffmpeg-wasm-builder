const runner = await BrowserFFmpeg.loadEmbedded({ coreJsText, wasmBytes });

const runInspect = async (bytes, name) => {
  const inputPath = `/workerfs/${name}`;
  const reportPath = `/report-${name.replace(/[^A-Za-z0-9]/g, "_")}.json`;
  const result = await runner.run({
    files: [{ name: inputPath, data: new Blob([bytes]), workerfs: true }],
    outputs: [reportPath],
    args: BrowserFFmpeg.videoAudioExtractorInspectArgs({ input: inputPath, output: reportPath }),
    onLog: ({ message }) => append(message)
  });
  if (result.exitCode !== 0) throw new Error(`inspect ${name} exit=${result.exitCode}`);
  return BrowserFFmpeg.decodeJsonOutput(result, reportPath);
};

const runCopy = async (bytes, name, streamIndex, format, outputPath) => {
  const inputPath = `/workerfs/${name}`;
  const result = await runner.run({
    files: [{ name: inputPath, data: new Blob([bytes]), workerfs: true }],
    outputs: [outputPath],
    args: BrowserFFmpeg.videoAudioExtractorCopyArgs({
      input: inputPath,
      output: outputPath,
      streamIndex,
      format
    }),
    onLog: ({ message }) => append(message)
  });
  if (result.exitCode !== 0) throw new Error(`copy ${name}/${format} exit=${result.exitCode}`);
  const file = result.files?.find((item) => item.name === outputPath);
  if (!file?.data?.byteLength) throw new Error(`copy ${name}/${format} returned no output`);
  return file.data;
};

append("case=mp4-aac start");
const mp4Report = await runInspect(input, "input.mp4");
if (mp4Report.schemaVersion !== 1) throw new Error("unexpected extractor report schema");
if (mp4Report.format?.videoStreamCount !== 1 || mp4Report.format?.audioStreamCount !== 1) throw new Error("MP4 stream counts are wrong");
const mp4Aac = mp4Report.audioStreams?.[0];
if (mp4Aac?.codec?.name !== "aac" || mp4Aac?.copy?.format !== "m4a") throw new Error("MP4 AAC copy recommendation is wrong");
const m4a = await runCopy(input, "input.mp4", mp4Aac.index, "m4a", "/output.m4a");
if (String.fromCharCode(...m4a.slice(4, 8)) !== "ftyp") throw new Error("M4A output has no ftyp box");
if (!containsAscii(m4a, "moov") || !containsAscii(m4a, "mdat") || !containsAscii(m4a, "mp4a")) throw new Error("M4A output is incomplete");

append("case=webm-opus start");
const webm = decodeFixture("webmOpus");
const webmReport = await runInspect(webm, "opus.webm");
const opus = webmReport.audioStreams?.find((stream) => stream.codec?.name === "opus");
if (!opus || opus.copy?.format !== "opus") throw new Error("WebM Opus was not recognized for stream copy");
const opusOut = await runCopy(webm, "opus.webm", opus.index, "opus", "/output.opus");
if (!containsAscii(opusOut, "OggS") || !containsAscii(opusOut, "OpusHead")) throw new Error("Ogg Opus output is invalid");

append("case=mkv-multi-audio start");
const mkv = decodeFixture("mkvMulti");
const mkvReport = await runInspect(mkv, "multi.mkv");
if (mkvReport.format?.videoStreamCount !== 1 || mkvReport.format?.audioStreamCount !== 3) throw new Error("MKV multi-audio stream counts are wrong");
const japanese = mkvReport.audioStreams.find((stream) => stream.language === "jpn" && stream.title === "日本語");
const english = mkvReport.audioStreams.find((stream) => stream.language === "eng" && stream.title === "English");
const commentary = mkvReport.audioStreams.find((stream) => stream.title === "Commentary");
if (!japanese?.default || japanese.codec?.name !== "aac") throw new Error("MKV default Japanese AAC metadata is wrong");
if (!english || english.codec?.name !== "opus") throw new Error("MKV English Opus metadata is wrong");
if (!commentary || commentary.codec?.name !== "aac") throw new Error("MKV Commentary AAC metadata is wrong");
await runCopy(mkv, "multi.mkv", english.index, "opus", "/english.opus");

append("case=mpegts-aac start");
const ts = decodeFixture("mpegTsAac");
const tsReport = await runInspect(ts, "input.ts");
const tsAac = tsReport.audioStreams?.find((stream) => stream.codec?.name === "aac");
if (!tsAac || tsAac.copy?.format !== "m4a") throw new Error("MPEG-TS AAC was not recognized");
const tsM4a = await runCopy(ts, "input.ts", tsAac.index, "m4a", "/from-ts.m4a");
if (String.fromCharCode(...tsM4a.slice(4, 8)) !== "ftyp" || !containsAscii(tsM4a, "mp4a")) throw new Error("MPEG-TS AAC -> M4A failed");

append("case=no-audio start");
const videoOnly = decodeFixture("videoOnly");
const noAudio = await runInspect(videoOnly, "video-only.mp4");
if (noAudio.format?.videoStreamCount !== 1 || noAudio.format?.audioStreamCount !== 0 || noAudio.audioStreams?.length !== 0) throw new Error("Video-only detection is wrong");

runner.dispose();
pass("cases=5_streamcopy=4");
