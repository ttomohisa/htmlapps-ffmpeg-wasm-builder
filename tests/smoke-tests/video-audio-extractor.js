const fromBase64 = (text) => {
  const raw = atob(text);
  const bytes = new Uint8Array(raw.length);
  for (let i = 0; i < raw.length; i++) bytes[i] = raw.charCodeAt(i);
  return bytes;
};

const fixtures = {
  mp4: input,
  webm: fromBase64("GkXfo59ChoEBQveBAULygQRC84EIQoKEd2VibUKHgQRChYECGFOAZwEAAAAAAAMlEU2bdLpNu4tTq4QVSalmU6yBoU27i1OrhBZUrmtTrIHWTbuMU6uEElTDZ1OsggGJTbuMU6uEHFO7a1OsggMP7AEAAAAAAABZAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAVSalmsCrXsYMPQkBNgIxMYXZmNjEuNy4xMDNXQYxMYXZmNjEuNy4xMDNEiYhAj0AAAAAAABZUrmtAra4BAAAAAAAAP9eBAXPFiLTfVso4XBpsnIEAIrWcg3VuZIiBAIaFVl9WUDmDgQEj44OEO5rKAOCQsIEQuoEQmoECVbCEVbmBAa4BAAAAAAAAXNeBAnPFiG4s9iqXqhXTnIEAIrWcg3VuZIiBAIaGQV9PUFVTVqqDYy6gVruEBMS0AIOBAuGRn4EBtYhAv0AAAAAAAGJkgRBjopNPcHVzSGVhZAEBOAFAHwAAAAAAElTDZ0DZc3OfY8CAZ8iZRaOHRU5DT0RFUkSHjExhdmY2MS43LjEwM3Nz2mPAi2PFiLTfVso4XBpsZ8ilRaOHRU5DT0RFUkSHmExhdmM2MS4xOS4xMDEgbGlidnB4LXZwOWfIoUWjiERVUkFUSU9ORIeTMDA6MDA6MDEuMDAwMDAwMDAwAHNz12PAi2PFiG4s9iqXqhXTZ8iiRaOHRU5DT0RFUkSHlUxhdmM2MS4xOS4xMDEgbGlib3B1c2fIoUWjiERVUkFUSU9ORIeTMDA6MDA6MDAuMTI4MDAwMDAwAB9DtnVAoeeBAKOLggAAgAgL5jsjq2CjvoEAAICCSYNCAADwAPYGOCQcGEIAACBAACKb//+lE/uClN6PFUq3bSf9VP0Z7ZRS03HSbMj+pvdlTLwmDIAAo4qCABWACAissw7Go4qCACmACAissw7Go4qCAD2ACAissw7Go4qCAFGACAissw7Go4qCAGWACAissw7GoJOhioIAeQAICKyzDsZ1ooQAzf5gHFO7a5G7j7OBALeK94EB8YICaPCBEA=="),
  mkv: fromBase64("GkXfo6NChoEBQveBAULygQRC84EIQoKIbWF0cm9za2FCh4EEQoWBAhhTgGcBAAAAAAAHgBFNm3TAv4SGGS5fTbuLU6uEFUmpZlOsgaFNu4tTq4QWVK5rU6yB7027jFOrhBJUw2dTrIICtE27jFOrhBxTu2tTrIIHZOwBAAAAAAAAUwAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAFUmpZsm/hCD4PA8q17GDD0JATYCMTGF2ZjYxLjcuMTAzV0GMTGF2ZjYxLjcuMTAzc6SQb+7FYxS3zxF73EQ8QLHmcUSJiECPQAAAAAAAFlSua0G/v4QPEtTGrgEAAAAAAACA14EBc8WIDmOGRQmRX9acgQAitZyDdW5kiIEAho9WX01QRUc0L0lTTy9BVkODgQEj44OEO5rKAOCQsIEQuoEQmoECVbCEVbmBAVXugQDsAQAAAAAAAAIAAGOipQFCwAr/4QAVZ0LACtp7ARAAAAMAEAAAAwAg8SJqAQAFaM4BlyCuAQAAAAAAAFPXgQJzxYhS7Gont6NrlZyBAFNuiEphcGFuZXNlIrWcg2pwboaFQV9BQUNWqoQHoSAAg4EC4ZGfgQG1iEC/QAAAAAAAYmSBIFXugQBjooUViFblAK4BAAAAAAAAateBA3PFiE7a2jT0LK/bnIEAU26HRW5nbGlzaCK1nINlbmeIgQCGhkFfT1BVU1aqg2MuoFa7hATEtACDgQLhkZ+BAbWIQL9AAAAAAABiZIEQVe6BAGOik09wdXNIZWFkAQE4AUAfAAAAAACuAQAAAAAAAFjXgQRzxYj6jaXEQIUyT5yBAFNuikNvbW1lbnRhcnkitZyDanBuiIEAhoVBX0FBQ1aqhAehIACDgQLhkZ+BAbWIQL9AAAAAAABiZIEgVe6BAGOihRWIVuUAElTDZ0GIv4TDk1AKc3OfY8CAZ8iZRaOHRU5DT0RFUkSHjExhdmY2MS43LjEwM3Nz12PAi2PFiA5jhkUJkV/WZ8iiRaOHRU5DT0RFUkSHlUxhdmM2MS4xOS4xMDEgbGlieDI2NGfIoUWjiERVUkFUSU9ORIeTMDA6MDA6MDEuMDAwMDAwMDAwAHNz02PAi2PFiFLsaie3o2uVZ8ieRaOHRU5DT0RFUkSHkUxhdmM2MS4xOS4xMDEgYWFjZ8ihRaOIRFVSQVRJT05Eh5MwMDowMDowMC4yNDgwMDAwMDAAc3PXY8CLY8WITtraNPQsr9tnyKJFo4dFTkNPREVSRIeVTGF2YzYxLjE5LjEwMSBsaWJvcHVzZ8ihRaOIRFVSQVRJT05Eh5MwMDowMDowMC4xMjgwMDAwMDAAc3PTY8CLY8WI+o2lxECFMk9nyJ5Fo4dFTkNPREVSRIeRTGF2YzYxLjE5LjEwMSBhYWNnyKFFo4hEVVJBVElPTkSHkzAwOjAwOjAwLjI0ODAwMDAwMAAfQ7Z1Qxy/hNl6MErngQCjmYIAAIDeAgBMYXZjNjEuMTkuMTAxAAIwQA6jmYQAAIDeAgBMYXZjNjEuMTkuMTAxAAIwQA6ji4MAAIAIC+Y7I6tgo0JogQAAgAAAAlMGBf//T9xF6b3m2Ui3lizYINkj7u94MjY0IC0gY29yZSAxNjQgcjMxMDggMzFlMTlmOSAtIEguMjY0L01QRUctNCBBVkMgY29kZWMgLSBDb3B5bGVmdCAyMDAzLTIwMjMgLSBodHRwOi8vd3d3LnZpZGVvbGFuLm9yZy94MjY0Lmh0bWwgLSBvcHRpb25zOiBjYWJhYz0wIHJlZj0xIGRlYmxvY2s9MDowOjAgYW5hbHlzZT0wOjAgbWU9ZGlhIHN1Ym1lPTAgcHN5PTEgcHN5X3JkPTEuMDA6MC4wMCBtaXhlZF9yZWY9MCBtZV9yYW5nZT0xNiBjaHJvbWFfbWU9MSB0cmVsbGlzPTAgOHg4ZGN0PTAgY3FtPTAgZGVhZHpvbmU9MjEsMTEgZmFzdF9wc2tpcD0xIGNocm9tYV9xcF9vZmZzZXQ9MCB0aHJlYWRzPTEgbG9va2FoZWFkX3RocmVhZHM9MSBzbGljZWRfdGhyZWFkcz0wIG5yPTAgZGVjaW1hdGU9MSBpbnRlcmxhY2VkPTAgYmx1cmF5X2NvbXBhdD0wIGNvbnN0cmFpbmVkX2ludHJhPTAgYmZyYW1lcz0wIHdlaWdodHA9MCBrZXlpbnQ9MjUwIGtleWludF9taW49MSBzY2VuZWN1dD0wIGludHJhX3JlZnJlc2g9MCByYz1jcmYgbWJ0cmVlPTAgY3JmPTUxLjAgcWNvbXA9MC42MCBxcG1pbj0wIHFwbWF4PTY5IHFwc3RlcD00IGlwX3JhdGlvPTEuNDAgYXE9MACAAAAACWWIhDomKAAVwKOIggCAgAEYIAejiIQAgIABGCAHo4qDABWACAissw7Go4qDACmACAissw7Go4qDAD2ACAissw7Go4qDAFGACAissw7Go4qDAGWACAissw7GoJOhioMAeQAICKyzDsZ1ooQAzf5gHFO7a5e/hCoB28e7j7OBALeK94EB8YIEQvCBTA=="),
  // Complete 0.3 s MPEG-TS AAC fixture (3572 bytes, SHA-256 95220ce6232b6633d82e5469b4b29e4aa6e1a12324018405cd81ccd450bcfb54).
  // Kept intact so TS sync detection, PES timestamps, and ADTS framing remain valid.
  ts: fromBase64("R0AREABC8CUAAcEAAP8B/wAB/IAUSBIBBkZGbXBlZwlTZXJ2aWNlMDF3fEPK//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////9HQAAQAACwDQABwQAAAAHwACqxBLL//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////0dQABAAArASAAHBAADhAPAAD+EA8AC2m8DZ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////R0EAMAdQAAB7DH4AAAABwAsYgIAFIQAH2GH/8UxAIB/83gIATGF2YzYxLjE5LjEwMQACYKpSLLSmKi9CQdCrj59lVOe/rzLLxJNySTcQgdkgbNWzXKs5NGcVYcVjbNjbNlOYtw2V0znIgAf6GM9w2t+NR3UOvta4t1XhuB17jvDbTt2NxWNxVxnqzHYnHWGtTtaatlLZSqUqlKpSqUqlKpSqUqlKpSaUmlKpSqMqjJqSaRpGdnaSZ5nZ2dpHAQARRpGdnZ2dpGkZ2dnZ2eZ2dnZ2dnbSwMDAwMillllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllliiiiiiii4P/xTEAdf/wBGpja2j3WYiYTZiJhN1MJf9P864jy1etR//F/3/Wya1rP4//i/7/rZNa1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dEIQDIQEYQDIgC4V+NV7c4/8f+P//3tplcVcnu9buJH6fj3a5DPNIg0VUVR/SD7jSN6QzntAOJuuXjHi5Ydfz7Y03mzN96bT1Bm+9M/yzL7xV4SHhF6xesq8JDwkPCL1i9YvWL1i+y/TIXrF6xesXrF6xesXrKKKRwEAHCiYmJib/zfcZdn+dZACIAAZ/1/SgekgAAGv9JsAAAA8thz/8UxAFd/8AOg1GJaoMxKDo5DohCAVCAlCAXEAXC8U/Gb3P+3/b//x8J3z7Tr3vz3HtJV1+eJ3wGGLs7O0809GNPRjT0Y09pHjY23QatIFAkKoozZYWrjXY2kcxYjbVOz18vddzwmWmWgiCJlplplplplplqpqsm759U7Ozs7Ozs7YY4Y4Y4Yuzs7OzsCECECECd23kzRHAQAdfqkACIAAPbwANfuAAAAZ/cgAAADwNIOA//FMQBk//AEKVS3UViILVIQQsHQqTIze/+n/rwrUpu43H1JJJK1+q39v/MOyNiUw6EM38b/c/W/qf5Pm91flCACkFwSC4BCNNIRJhCthOnMgkkdrJ5LCE7M8nHgE1EJoBqb0z/gQAD/w++c0VgWz7X9/9/1vK39JZNo2jKLBGEDt5+fn25tDszzzzlGLssssoRbM885VCIuDz7gAAYcPD0cBAB5YAxaHh7dgAAK4SMPW4AAMWh4e3YAADDh4e2/F4AM85VCMsssog1sz4P/xTEAcP/wBGJmmzDZFRN+h6Hr/t/z5mpNa1n2//i/7/XXHQBAZQDGXuqrjv1gdmTav00MZnQX5cFUzeftPF6kIyPF2epBSef6fSKI4+n0iKcD6fT6RRAAc/0+hRCdQ+n0+kURQAQHPtQSSMeUSm246EUTrnQfHZdc6hZw/3MLnGPj2aPGzh7eiSw4Cezi1RwEAP1kA/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////yihLebqkSgBx627l2alRUlFFUx6xLRRKBGhRIUSEaZhSqGVgeY9RdWimCjCmcC6N8i6B/R/QNMNMNDd+7N35szfmwUw0wCgC87wvO8Lzqiq6o7/8UxAAZ/8ARiBtHA="),
  noAudio: fromBase64("AAAAIGZ0eXBpc29tAAACAGlzb21pc28yYXZjMW1wNDEAAAMNbW9vdgAAAGxtdmhkAAAAAAAAAAAAAAAAAAAD6AAAA+gAAQAAAQAAAAAAAAAAAAAAAAEAAAAAAAAAAAAAAAAAAAABAAAAAAAAAAAAAAAAAABAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAgAAAjh0cmFrAAAAXHRraGQAAAADAAAAAAAAAAAAAAABAAAAAAAAA+gAAAAAAAAAAAAAAAAAAAAAAAEAAAAAAAAAAAAAAAAAAAABAAAAAAAAAAAAAAAAAABAAAAAABAAAAAQAAAAAAAkZWR0cwAAABxlbHN0AAAAAAAAAAEAAAPoAAAAAAABAAAAAAGwbWRpYQAAACBtZGhkAAAAAAAAAAAAAAAAAABAAAAAQABVxAAAAAAALWhkbHIAAAAAAAAAAHZpZGUAAAAAAAAAAAAAAABWaWRlb0hhbmRsZXIAAAABW21pbmYAAAAUdm1oZAAAAAEAAAAAAAAAAAAAACRkaW5mAAAAHGRyZWYAAAAAAAAAAQAAAAx1cmwgAAAAAQAAARtzdGJsAAAAt3N0c2QAAAAAAAAAAQAAAKdhdmMxAAAAAAAAAAEAAAAAAAAAAAAAAAAAAAAAABAAEABIAAAASAAAAAAAAAABFUxhdmM2MS4xOS4xMDEgbGlieDI2NAAAAAAAAAAAAAAAGP//AAAALWF2Y0MBQsAK/+EAFWdCwAraewEQAAADABAAAAMAIPEiagEABWjOAZcgAAAAEHBhc3AAAAABAAAAAQAAABRidHJ0AAAAAAAAEyAAAAAAAAAAGHN0dHMAAAAAAAAAAQAAAAEAAEAAAAAAHHN0c2MAAAAAAAAAAQAAAAEAAAABAAAAAQAAABRzdHN6AAAAAAAAAmQAAAABAAAAFHN0Y28AAAAAAAAAAQAAAz0AAABhdWR0YQAAAFltZXRhAAAAAAAAACFoZGxyAAAAAAAAAABtZGlyYXBwbAAAAAAAAAAAAAAAACxpbHN0AAAAJKl0b28AAAAcZGF0YQAAAAEAAAAATGF2ZjYxLjcuMTAzAAAACGZyZWUAAAJsbWRhdAAAAlMGBf//T9xF6b3m2Ui3lizYINkj7u94MjY0IC0gY29yZSAxNjQgcjMxMDggMzFlMTlmOSAtIEguMjY0L01QRUctNCBBVkMgY29kZWMgLSBDb3B5bGVmdCAyMDAzLTIwMjMgLSBodHRwOi8vd3d3LnZpZGVvbGFuLm9yZy94MjY0Lmh0bWwgLSBvcHRpb25zOiBjYWJhYz0wIHJlZj0xIGRlYmxvY2s9MDowOjAgYW5hbHlzZT0wOjAgbWU9ZGlhIHN1Ym1lPTAgcHN5PTEgcHN5X3JkPTEuMDA6MC4wMCBtaXhlZF9yZWY9MCBtZV9yYW5nZT0xNiBjaHJvbWFfbWU9MSB0cmVsbGlzPTAgOHg4ZGN0PTAgY3FtPTAgZGVhZHpvbmU9MjEsMTEgZmFzdF9wc2tpcD0xIGNocm9tYV9xcF9vZmZzZXQ9MCB0aHJlYWRzPTEgbG9va2FoZWFkX3RocmVhZHM9MSBzbGljZWRfdGhyZWFkcz0wIG5yPTAgZGVjaW1hdGU9MSBpbnRlcmxhY2VkPTAgYmx1cmF5X2NvbXBhdD0wIGNvbnN0cmFpbmVkX2ludHJhPTAgYmZyYW1lcz0wIHdlaWdodHA9MCBrZXlpbnQ9MjUwIGtleWludF9taW49MSBzY2VuZWN1dD0wIGludHJhX3JlZnJlc2g9MCByYz1jcmYgbWJ0cmVlPTAgY3JmPTUxLjAgcWNvbXA9MC42MCBxcG1pbj0wIHFwbWF4PTY5IHFwc3RlcD00IGlwX3JhdGlvPTEuNDAgYXE9MACAAAAACWWIhDomKAAVwA==")
};


const compatibilityFixtures = {
  mp3: { extension: "mkv", bytes: fromBase64("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") },
  flac: { extension: "mkv", bytes: fromBase64("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") },
  alac: { extension: "mov", bytes: fromBase64("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") },
  vorbis: { extension: "mkv", bytes: fromBase64("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") }
};

const runner = await BrowserFFmpeg.loadEmbedded({ coreJsText, wasmBytes });
const inspect = async (label, bytes, extension) => {
  append("inspect=" + label);
  const inputPath = "/workerfs/" + label + "." + extension;
  const reportPath = "/report-" + label + ".json";
  const result = await runner.run({
    files: [{ name: inputPath, data: new Blob([bytes]), workerfs: true }],
    outputs: [reportPath],
    args: BrowserFFmpeg.videoAudioExtractorInspectArgs({ input: inputPath, output: reportPath }),
    onLog: ({ message }) => append(message)
  });
  return BrowserFFmpeg.decodeJsonOutput(result, reportPath);
};
const copy = async (label, bytes, extension, stream, format, outputPath) => {
  append("copy=" + label + " stream=" + stream + " format=" + format);
  const inputPath = "/workerfs/" + label + "." + extension;
  return await runner.run({
    files: [{ name: inputPath, data: new Blob([bytes]), workerfs: true }],
    outputs: [outputPath],
    args: BrowserFFmpeg.videoAudioExtractorCopyArgs({
      input: inputPath, streamIndex: stream, format, output: outputPath
    }),
    onLog: ({ message }) => append(message)
  });
};
const transcode = async (label, bytes, extension, stream, format, outputPath, bitrateKbps, channelMode) => {
  append("transcode=" + label + " stream=" + stream + " format=" + format);
  const inputPath = "/workerfs/" + label + "." + extension;
  return await runner.run({
    files: [{ name: inputPath, data: new Blob([bytes]), workerfs: true }],
    outputs: [outputPath],
    args: BrowserFFmpeg.videoAudioExtractorTranscodeArgs({
      input: inputPath, streamIndex: stream, format, output: outputPath, bitrateKbps, channelMode
    }),
    onLog: ({ message }) => append(message)
  });
};
try {
  const mp4 = await inspect("mp4-aac", fixtures.mp4, "mp4");
  if (mp4.format.videoStreamCount < 1 || mp4.format.audioStreamCount !== 1) throw new Error("MP4 stream counts are wrong.");
  const mp4Audio = mp4.audioStreams[0];
  if (mp4Audio.codec.name !== "aac" || mp4Audio.copy.format !== "m4a") throw new Error("MP4 AAC copy mapping is wrong.");
  const m4a = await copy("mp4-aac", fixtures.mp4, "mp4", mp4Audio.index, "m4a", "/mp4-aac.m4a");
  const m4aBytes = m4a.files[0].data;
  if (!containsAscii(m4aBytes, "ftyp") || !containsAscii(m4aBytes, "mp4a")) throw new Error("AAC -> M4A output is invalid.");

  const webmReport = await inspect("webm-opus", fixtures.webm, "webm");
  const opus = webmReport.audioStreams[0];
  if (webmReport.format.videoStreamCount !== 1 || opus?.codec?.name !== "opus" || opus?.copy?.format !== "opus") {
    throw new Error("WebM Opus inspection failed.");
  }
  const opusResult = await copy("webm-opus", fixtures.webm, "webm", opus.index, "opus", "/webm-opus.opus");
  const opusBytes = opusResult.files[0].data;
  if (!containsAscii(opusBytes, "OggS") || !containsAscii(opusBytes, "OpusHead")) throw new Error("Opus output is invalid.");

  const multi = await inspect("multi-audio", fixtures.mkv, "mkv");
  if (multi.format.videoStreamCount !== 1 || multi.format.audioStreamCount !== 3 || multi.audioStreams.length !== 3) {
    throw new Error("MKV audio stream inventory is incomplete.");
  }
  const japanese = multi.audioStreams.find((s) => s.language === "jpn" && s.title === "Japanese");
  const english = multi.audioStreams.find((s) => s.language === "eng" && s.title === "English");
  const commentary = multi.audioStreams.find((s) => s.title === "Commentary");
  if (!japanese?.default || english?.codec?.name !== "opus" || commentary?.codec?.name !== "aac") {
    throw new Error("MKV language/title/default metadata was not preserved.");
  }
  const englishResult = await copy("multi-audio", fixtures.mkv, "mkv", english.index, "opus", "/english.opus");
  if (!containsAscii(englishResult.files[0].data, "OpusHead")) throw new Error("Selected MKV Opus track was not copied.");

  const m4aTranscoded = await transcode("multi-opus-to-m4a", fixtures.mkv, "mkv", english.index, "m4a", "/english-aac.m4a", 192);
  const m4aTranscodedBytes = m4aTranscoded.files[0].data;
  if (!containsAscii(m4aTranscodedBytes, "ftyp") || !containsAscii(m4aTranscodedBytes, "mp4a")) {
    throw new Error("Opus -> AAC/M4A transcode failed.");
  }
  const m4aTranscodedReport = await inspect("transcoded-m4a", m4aTranscodedBytes, "m4a");
  if (m4aTranscodedReport.audioStreams[0]?.codec?.name !== "aac") {
    throw new Error("Transcoded M4A does not contain AAC.");
  }

  const wavTranscoded = await transcode("multi-aac-to-wav", fixtures.mkv, "mkv", commentary.index, "wav", "/commentary.wav");
  const wavTranscodedBytes = wavTranscoded.files[0].data;
  if (!containsAscii(wavTranscodedBytes, "RIFF") || !containsAscii(wavTranscodedBytes, "WAVE")) {
    throw new Error("AAC -> PCM16/WAV transcode failed.");
  }
  const wavTranscodedReport = await inspect("transcoded-wav", wavTranscodedBytes, "wav");
  if (wavTranscodedReport.audioStreams[0]?.codec?.name !== "pcm_s16le") {
    throw new Error("Transcoded WAV is not PCM signed 16-bit little-endian.");
  }

  for (const bitrateKbps of [128, 192, 256, 320]) {
    const mp3Transcoded = await transcode(
      "multi-opus-to-mp3-" + bitrateKbps,
      fixtures.mkv, "mkv", english.index, "mp3",
      "/english-" + bitrateKbps + ".mp3", bitrateKbps, "stereo"
    );
    const mp3TranscodedBytes = mp3Transcoded.files[0].data;
    const mp3TranscodedReport = await inspect(
      "transcoded-mp3-" + bitrateKbps, mp3TranscodedBytes, "mp3"
    );
    const encoded = mp3TranscodedReport.audioStreams[0];
    if (encoded?.codec?.name !== "mp3" || encoded?.channels !== 2) {
      throw new Error("LAME MP3 " + bitrateKbps + " kbps stereo transcode failed.");
    }
  }

  const mp3Mono = await transcode(
    "multi-opus-to-mp3-mono",
    fixtures.mkv, "mkv", english.index, "mp3",
    "/english-mono.mp3", 192, "mono"
  );
  const mp3MonoReport = await inspect("transcoded-mp3-mono", mp3Mono.files[0].data, "mp3");
  if (mp3MonoReport.audioStreams[0]?.codec?.name !== "mp3" ||
      mp3MonoReport.audioStreams[0]?.channels !== 1) {
    throw new Error("LAME MP3 mono transcode failed.");
  }

  const tsReport = await inspect("mpegts-aac", fixtures.ts, "ts");
  const tsAudio = tsReport.audioStreams.find((s) => s.codec?.name === "aac");
  if (!tsAudio || tsAudio.copy?.format !== "m4a" || tsAudio.sampleRate !== 48000 || tsAudio.channels !== 1) {
    throw new Error("MPEG-TS AAC mapping failed.");
  }
  const tsM4a = await copy("mpegts-aac", fixtures.ts, "ts", tsAudio.index, "m4a", "/mpegts-aac.m4a");
  if (!containsAscii(tsM4a.files[0].data, "ftyp") || !containsAscii(tsM4a.files[0].data, "mp4a")) {
    throw new Error("MPEG-TS AAC -> M4A failed.");
  }


  const mp3Report = await inspect("compat-mp3", compatibilityFixtures.mp3.bytes, compatibilityFixtures.mp3.extension);
  const mp3Audio = mp3Report.audioStreams[0];
  if (mp3Audio?.codec?.name !== "mp3" || mp3Audio?.copy?.format !== "mp3") throw new Error("MP3 copy mapping failed.");
  const mp3Result = await copy("compat-mp3", compatibilityFixtures.mp3.bytes, compatibilityFixtures.mp3.extension, mp3Audio.index, "mp3", "/compat.mp3");
  if (!containsAscii(mp3Result.files[0].data, "ID3")) throw new Error("MP3 output is invalid.");

  const flacReport = await inspect("compat-flac", compatibilityFixtures.flac.bytes, compatibilityFixtures.flac.extension);
  const flacAudio = flacReport.audioStreams[0];
  if (flacAudio?.codec?.name !== "flac" || flacAudio?.copy?.format !== "flac") throw new Error("FLAC copy mapping failed.");
  const flacResult = await copy("compat-flac", compatibilityFixtures.flac.bytes, compatibilityFixtures.flac.extension, flacAudio.index, "flac", "/compat.flac");
  if (!containsAscii(flacResult.files[0].data, "fLaC")) throw new Error("FLAC output is invalid.");

  const alacReport = await inspect("compat-alac", compatibilityFixtures.alac.bytes, compatibilityFixtures.alac.extension);
  const alacAudio = alacReport.audioStreams[0];
  if (alacAudio?.codec?.name !== "alac" || alacAudio?.copy?.format !== "m4a") throw new Error("ALAC copy mapping failed.");
  const alacResult = await copy("compat-alac", compatibilityFixtures.alac.bytes, compatibilityFixtures.alac.extension, alacAudio.index, "m4a", "/compat-alac.m4a");
  if (!containsAscii(alacResult.files[0].data, "ftyp") || !containsAscii(alacResult.files[0].data, "alac")) throw new Error("ALAC -> M4A output is invalid.");

  const vorbisReport = await inspect("compat-vorbis", compatibilityFixtures.vorbis.bytes, compatibilityFixtures.vorbis.extension);
  const vorbisAudio = vorbisReport.audioStreams[0];
  if (vorbisAudio?.codec?.name !== "vorbis" || vorbisAudio?.copy?.format !== "ogg") throw new Error("Vorbis copy mapping failed.");
  const vorbisResult = await copy("compat-vorbis", compatibilityFixtures.vorbis.bytes, compatibilityFixtures.vorbis.extension, vorbisAudio.index, "ogg", "/compat-vorbis.ogg");
  if (!containsAscii(vorbisResult.files[0].data, "OggS") || !containsAscii(vorbisResult.files[0].data, "vorbis")) throw new Error("Vorbis -> OGG output is invalid.");

  const noAudioReport = await inspect("video-only", fixtures.noAudio, "mp4");
  if (noAudioReport.format.videoStreamCount !== 1 || noAudioReport.format.audioStreamCount !== 0 ||
      noAudioReport.audioStreams.length !== 0) {
    throw new Error("Video-only input was not reported correctly.");
  }

  pass("phase4_copy_and_transcode_mp3");
} finally {
  runner.dispose();
}
