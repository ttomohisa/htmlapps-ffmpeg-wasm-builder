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
  ts: fromBase64("R0AREABC8CUAAcEAAP8B/wAB/IAUSBIBBkZGbXBlZwlTZXJ2aWNlMDF3fEPK//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////9HQAAQAACwDQABwQAAAAHwACqxBLL//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////0dQABAAArAXAAHBAADhAPAAG+EA8AAP4QHwAC9EuZv/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////R0EAMAdQAACRjH4AAAAB4AAAgIAFIQAJMmEAAAABCfAAAAABZ0LACtp7ARAAAAMAEAAAAwAg8SJqAAAAAWjOAZcgAAABBgX//0/cRem95tlIt5Ys2CDZI+7veDI2NCAtIGNvcmUgMTY0IHIzMTA4IDMxZTE5ZjkgLSBILjI2NC9NUEVHLTQgQVZDIGNvZGVjIC0gQ29weWxlZnQgMjAwMy0yMDIzIC0gaHR0cDovL3d3dy52aWRlb2xhbi5HAQARb3JnL3gyNjQuaHRtbCAtIG9wdGlvbnM6IGNhYmFjPTAgcmVmPTEgZGVibG9jaz0wOjA6MCBhbmFseXNlPTA6MCBtZT1kaWEgc3VibWU9MCBwc3k9MSBwc3lfcmQ9MS4wMDowLjAwIG1peGVkX3JlZj0wIG1lX3JhbmdlPTE2IGNocm9tYV9tZT0xIHRyZWxsaXM9MCA4eDhkY3Q9MCBjcW09MCBkZWFkem9uZT0yMSwxMSBmYXN0X0cBABJwc2tpcD0xIGNocm9tYV9xcF9vZmZzZXQ9MCB0aHJlYWRzPTEgbG9va2FoZWFkX3RocmVhZHM9MSBzbGljZWRfdGhyZWFkcz0wIG5yPTAgZGVjaW1hdGU9MSBpbnRlcmxhY2VkPTAgYmx1cmF5X2NvbXBhdD0wIGNvbnN0cmFpbmVkX2ludHJhPTAgYmZyYW1lcz0wIHdlaWdodHA9MCBrZXlpbnQ9MjUwIGtleWludF9taW49MSBzRwEAMz8A//////////////////////////////////////////////////////////////////////////////////9jZW5lY3V0PTAgaW50cmFfcmVmcmVzaD0wIHJjPWNyZiBtYnRyZWU9MCBjcmY9NTEuMCBxY29tcD0wLjYwIHFwbWluPTAgcXBtYXg9NjkgcXBzdGVwPTQgaXBfcmF0aW89MS40MCBhcT0wAIAAAAFliIQ6JigAFcBHQQEwgkD///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////8AAAHAAC+AgAUhAAfYYf/xbEADn/zeAgBMYXZjNjEuMTkuMTAxAAIwQA7/8WxAAX/8ARggBw=="),
  noAudio: fromBase64("AAAAIGZ0eXBpc29tAAACAGlzb21pc28yYXZjMW1wNDEAAAMNbW9vdgAAAGxtdmhkAAAAAAAAAAAAAAAAAAAD6AAAA+gAAQAAAQAAAAAAAAAAAAAAAAEAAAAAAAAAAAAAAAAAAAABAAAAAAAAAAAAAAAAAABAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAgAAAjh0cmFrAAAAXHRraGQAAAADAAAAAAAAAAAAAAABAAAAAAAAA+gAAAAAAAAAAAAAAAAAAAAAAAEAAAAAAAAAAAAAAAAAAAABAAAAAAAAAAAAAAAAAABAAAAAABAAAAAQAAAAAAAkZWR0cwAAABxlbHN0AAAAAAAAAAEAAAPoAAAAAAABAAAAAAGwbWRpYQAAACBtZGhkAAAAAAAAAAAAAAAAAABAAAAAQABVxAAAAAAALWhkbHIAAAAAAAAAAHZpZGUAAAAAAAAAAAAAAABWaWRlb0hhbmRsZXIAAAABW21pbmYAAAAUdm1oZAAAAAEAAAAAAAAAAAAAACRkaW5mAAAAHGRyZWYAAAAAAAAAAQAAAAx1cmwgAAAAAQAAARtzdGJsAAAAt3N0c2QAAAAAAAAAAQAAAKdhdmMxAAAAAAAAAAEAAAAAAAAAAAAAAAAAAAAAABAAEABIAAAASAAAAAAAAAABFUxhdmM2MS4xOS4xMDEgbGlieDI2NAAAAAAAAAAAAAAAGP//AAAALWF2Y0MBQsAK/+EAFWdCwAraewEQAAADABAAAAMAIPEiagEABWjOAZcgAAAAEHBhc3AAAAABAAAAAQAAABRidHJ0AAAAAAAAEyAAAAAAAAAAGHN0dHMAAAAAAAAAAQAAAAEAAEAAAAAAHHN0c2MAAAAAAAAAAQAAAAEAAAABAAAAAQAAABRzdHN6AAAAAAAAAmQAAAABAAAAFHN0Y28AAAAAAAAAAQAAAz0AAABhdWR0YQAAAFltZXRhAAAAAAAAACFoZGxyAAAAAAAAAABtZGlyYXBwbAAAAAAAAAAAAAAAACxpbHN0AAAAJKl0b28AAAAcZGF0YQAAAAEAAAAATGF2ZjYxLjcuMTAzAAAACGZyZWUAAAJsbWRhdAAAAlMGBf//T9xF6b3m2Ui3lizYINkj7u94MjY0IC0gY29yZSAxNjQgcjMxMDggMzFlMTlmOSAtIEguMjY0L01QRUctNCBBVkMgY29kZWMgLSBDb3B5bGVmdCAyMDAzLTIwMjMgLSBodHRwOi8vd3d3LnZpZGVvbGFuLm9yZy94MjY0Lmh0bWwgLSBvcHRpb25zOiBjYWJhYz0wIHJlZj0xIGRlYmxvY2s9MDowOjAgYW5hbHlzZT0wOjAgbWU9ZGlhIHN1Ym1lPTAgcHN5PTEgcHN5X3JkPTEuMDA6MC4wMCBtaXhlZF9yZWY9MCBtZV9yYW5nZT0xNiBjaHJvbWFfbWU9MSB0cmVsbGlzPTAgOHg4ZGN0PTAgY3FtPTAgZGVhZHpvbmU9MjEsMTEgZmFzdF9wc2tpcD0xIGNocm9tYV9xcF9vZmZzZXQ9MCB0aHJlYWRzPTEgbG9va2FoZWFkX3RocmVhZHM9MSBzbGljZWRfdGhyZWFkcz0wIG5yPTAgZGVjaW1hdGU9MSBpbnRlcmxhY2VkPTAgYmx1cmF5X2NvbXBhdD0wIGNvbnN0cmFpbmVkX2ludHJhPTAgYmZyYW1lcz0wIHdlaWdodHA9MCBrZXlpbnQ9MjUwIGtleWludF9taW49MSBzY2VuZWN1dD0wIGludHJhX3JlZnJlc2g9MCByYz1jcmYgbWJ0cmVlPTAgY3JmPTUxLjAgcWNvbXA9MC42MCBxcG1pbj0wIHFwbWF4PTY5IHFwc3RlcD00IGlwX3JhdGlvPTEuNDAgYXE9MACAAAAACWWIhDomKAAVwA==")
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

  const tsReport = await inspect("mpegts-aac", fixtures.ts, "ts");
  const tsAudio = tsReport.audioStreams.find((s) => s.codec?.name === "aac");
  if (!tsAudio || tsAudio.copy?.format !== "m4a") throw new Error("MPEG-TS AAC mapping failed.");
  const tsM4a = await copy("mpegts-aac", fixtures.ts, "ts", tsAudio.index, "m4a", "/mpegts-aac.m4a");
  if (!containsAscii(tsM4a.files[0].data, "ftyp") || !containsAscii(tsM4a.files[0].data, "mp4a")) {
    throw new Error("MPEG-TS AAC -> M4A failed.");
  }

  const noAudioReport = await inspect("video-only", fixtures.noAudio, "mp4");
  if (noAudioReport.format.videoStreamCount !== 1 || noAudioReport.format.audioStreamCount !== 0 ||
      noAudioReport.audioStreams.length !== 0) {
    throw new Error("Video-only input was not reported correctly.");
  }

  pass("mp4_aac_webm_opus_mkv_multi_mpegts_aac_no_audio");
} finally {
  runner.dispose();
}
