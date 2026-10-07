/* Filter Builder timing tests shared by actual Node/MEMFS and browser/WORKERFS cores.
 * Expectations come from the synthetic sources, never the output's guessed FPS.
 * Removing frame-duration propagation, changing half-open ranges, or retaining
 * stale pre-setpts durations must fail these cases in either input mode.
 */
(function (root) {
  "use strict";
  const assert = (value, message) => { if (!value) throw new Error(message); };
  const near = (actual, expected, tolerance, label) =>
    assert(Number.isFinite(actual) && Math.abs(actual - expected) <= tolerance + 1e-9,
      `${label}: expected ${expected}, got ${actual}`);
  const cfrPts = (count, fps) => Array.from({length: count}, (_, i) => i / fps);

  function checkTiming(bytes, expected) {
    const mp4 = root.TimingReaders.readMp4(bytes), video = mp4.video;
    assert(video.sampleCount === expected.pts.length,
      `coded frames: expected ${expected.pts.length}, got ${video.sampleCount}`);
    const pts = video.pts.slice().sort((a, b) => a - b), first = pts[0];
    pts.forEach((value, index) => near((value - first) / video.timescale,
      expected.pts[index], 1 / video.timescale, `PTS ${index}`));
    assert(video.durations.every(duration => duration > 0), "nonpositive sample interval (including terminal sample)");
    const visibleEdits = video.edits.filter(edit => edit.mediaTime >= 0);
    assert(visibleEdits.length === 1 && video.edits.length === 1, "expected one nonempty video edit without an initial gap");
    near((first - visibleEdits[0].mediaTime) / video.timescale, 0, 1 / video.timescale, "first visible PTS");
    assert(video.editPresentationEnd, "missing video edit-list coverage");
    near((video.editPresentationEnd.numerator / video.editPresentationEnd.denominator - first) / video.timescale,
      expected.endpoint, 1 / mp4.movieTimescale, "edit-list endpoint");
    if (!expected.vfr) {
      video.durations.forEach((duration, index) => near(duration / video.timescale,
        1 / expected.fps, 1 / video.timescale, `sample interval ${index}`));
      near(video.duration / video.timescale, expected.endpoint, 1 / video.timescale, "media duration");
      near((video.presentationEnd - first) / video.timescale, expected.endpoint, 1 / video.timescale, "presentation endpoint");
    }
    return video;
  }

  function checkDecoded(info, expected) {
    assert(info.codec === "h264", `expected H.264, got ${info.codec}`);
    assert(info.count === expected.pts.length, `${info.decodeKind || "normally decoded"} frames: expected ${expected.pts.length}, got ${info.count}`);
    assert(info.width === expected.width && info.height === expected.height,
      `geometry: expected ${expected.width}x${expected.height}, got ${info.width}x${info.height}`);
    assert(info.yuv420p, "output is not 8-bit 4:2:0 H.264");
    assert(info.lumaVariance > 80, `blank first frame: variance ${info.lumaVariance}`);
    assert(info.chromaError < 8, `grayscale content changed: chroma error ${info.chromaError}`);
    if (expected.sourceStart === 1) {
      assert(info.referenceControlRejected, "wrong-start negative control was not rejected");
      assertPreviewAlignment(info.referenceError);
    }
    if (expected.audio) {
      assert(info.audio, "AAC audio is missing");
      assert(info.audio.codec === "aac", "audio codec is not AAC");
      assert(info.audio.channels === expected.channels && info.audio.sampleRate === 48000, `audio channel/rate contract changed: expected ${expected.channels} channels at 48kHz`);
      near(info.audio.duration, expected.endpoint, 0.065, "audio endpoint (AAC/atempo tolerance)");
      near(info.audio.toneHz, 440, 5, "audio tone");
    } else assert(!info.audio, "video-only case unexpectedly has audio");
  }

  // The same decoded-pixel and tone oracles are used by the browser and Node.
  function pixelSummary(rgba, stride = 4) {
    const gray = [], values = [];
    let chromaError = 0;
    for (let i = 0; i < rgba.length; i += stride) {
      const r = rgba[i], g = rgba[i + 1], b = rgba[i + 2];
      chromaError += Math.max(r, g, b) - Math.min(r, g, b);
      values.push((r + g + b) / 3); gray.push(.299 * r + .587 * g + .114 * b);
    }
    const mean = values.reduce((a, b) => a + b, 0) / values.length;
    return {gray, lumaVariance: values.reduce((a, b) => a + (b - mean) ** 2, 0) / values.length,
      chromaError: chromaError / values.length};
  }
  // Compare source/output Y planes directly: Canvas RGB conversion can choose
  // different color matrices for untagged video. This retains the strict
  // spatial-alignment threshold and the wrong-start negative control.
  function lumaReference(bytes, plane, width, height, columns = 32, rows = 18) {
    assert(plane && Number.isInteger(plane.offset) && plane.offset >= 0 &&
      Number.isInteger(plane.stride) && plane.stride >= width && width >= columns && height >= rows &&
      plane.offset + (height - 1) * plane.stride + width <= bytes.length, "invalid decoded luma plane");
    const values = [];
    for (let y = 0; y < rows; y++) for (let x = 0; x < columns; x++) {
      const left = Math.floor(x * width / columns), right = Math.floor((x + 1) * width / columns);
      const top = Math.floor(y * height / rows), bottom = Math.floor((y + 1) * height / rows);
      let sum = 0;
      for (let row = top; row < bottom; row++) for (let col = left; col < right; col++)
        sum += bytes[plane.offset + row * plane.stride + col];
      values.push(sum / ((right - left) * (bottom - top)));
    }
    return values;
  }
  function referenceError(actual, reference) {
    assert(actual.length === reference.length, "reference sample geometry differs");
    return actual.reduce((sum, value, i) => sum + Math.abs(value - reference[i]), 0) / actual.length;
  }
  function assertPreviewAlignment(error) {
    // Native RGB and browser Y-grid references avoid an assumed display color
    // matrix. Correct Y-grid MAE is <0.55; wrong 0s/2s grids are >7.49.
    assert(Number.isFinite(error) && error < 4, `nonzero preview does not start at source 1s: pixel error ${error}`);
  }
  function rejectWrongStart(wrong, reference) {
    const error = referenceError(wrong, reference);
    try { assertPreviewAlignment(error); }
    catch (_) { return true; }
    throw new Error(`wrong-start negative control falsely passed with pixel error ${error}`);
  }
  function toneFrequency(pcm, rate) {
    const start = Math.min(Math.floor(rate * .25), Math.floor(pcm.length / 4));
    const length = Math.min(Math.floor(rate * .2), pcm.length - start);
    let best = 0, bestHz = 0;
    for (let hz = 420; hz <= 460; hz++) {
      let real = 0, imaginary = 0;
      for (let i = 0; i < length; i++) {
        const angle = 2 * Math.PI * hz * i / rate;
        real += pcm[start + i] * Math.cos(angle); imaginary += pcm[start + i] * Math.sin(angle);
      }
      const power = real * real + imaginary * imaginary;
      if (power > best) { best = power; bestHz = hz; }
    }
    assert(best > .01, "audio is silent");
    return bestHz;
  }

  async function runFilterBuilderTimingRegressions(fixtures, run, api, decode, log = () => {}) {
    assert(typeof decode === "function", "a real decoded-frame oracle is required");
    const cases = [], failures = [];
    for (const mode of ["single-input", "multi-input"]) {
      const add = (name, options = {}) => cases.push({name: `${mode}-${name}`, mode,
        fixture: "filterCfr", pts: cfrPts(180, 30), endpoint: 6, fps: 30,
        width: 320, height: 180, videoFilter: "hue=s=0", audio: false, channels: mode === "single-input" ? 1 : 2, ...options});
      add("full-grayscale", {audio: true});
      add("preview-0-3", {audio: true, duration: 3, pts: cfrPts(90, 30), endpoint: 3});
      add("preview-1-4", {audio: true, sourceStart: 1, duration: 3, pts: cfrPts(90, 30), endpoint: 3});
      add("scale160", {videoFilter: "hue=s=0,scale=160:90", width: 160, height: 90});
      for (const fps of [15, 12.5]) add(`fps-${fps}`, {videoFilter: `hue=s=0,fps=${fps}`, fps, pts: cfrPts(6 * fps, fps)});
      add("vfr", {fixture: "vfr", width: 64, height: 48, pts: [0, .083, .207, .249, .491, .532], endpoint: .573, vfr: true});
      add("single-frame", {fixture: "single", width: 64, height: 48, pts: [0], endpoint: .1, fps: 10});
      for (const speed of [1.5, .5]) add(`speed-${speed}`, {videoFilter: `hue=s=0,setpts=PTS/${speed}`,
        fps: 30 * speed, pts: cfrPts(180, 30 * speed), endpoint: 6 / speed});
      add("speed-then-fps", {videoFilter: "hue=s=0,setpts=PTS/1.5,fps=15", fps: 15, pts: cfrPts(60, 15), endpoint: 4});
      add("single-frame-speed", {fixture: "single", width: 64, height: 48, videoFilter: "hue=s=0,setpts=PTS/1.5", pts: [0], endpoint: 1 / 15, fps: 15});
      add("vfr-speed", {fixture: "vfr", width: 64, height: 48, videoFilter: "hue=s=0,setpts=PTS/1.5",
        pts: [0, .083, .207, .249, .491, .532].map(pts => pts / 1.5), endpoint: .573 / 1.5, vfr: true});
      for (const speed of [1, 1.5]) add(`vfr-distinct-tail-${speed}`, {fixture: "vfrTail", width: 64, height: 48,
        videoFilter: `hue=s=0,setpts=PTS/${speed}`, pts: [0, .083, .207, .249, .491, .532].map(pts => pts / speed),
        endpoint: .605 / speed, vfr: true});
      add("chained-speed", {videoFilter: "hue=s=0,setpts=PTS/1.5,setpts=PTS*2", fps: 22.5, pts: cfrPts(180, 22.5), endpoint: 8});
      add("unknown-then-fps", {videoFilter: "hue=s=0,setpts=N/(30*TB),fps=15", fps: 15, pts: cfrPts(90, 15)});
      add("unknown-expression-compatible", {videoFilter: "hue=s=0,setpts=N/(30*TB)", unknownDuration: true});
      add("speed-1.5-atempo", {videoFilter: "hue=s=0,setpts=PTS/1.5", audioFilter: "atempo=1.5", audio: true,
        fps: 45, pts: cfrPts(180, 45), endpoint: 4});
    }
    cases.push({name: "multi-input-mixed-timebase-overlay", mode: "multi-input", fixture: "filterCfr",
      secondFixture: "vfr", mixedOverlay: true, pts: cfrPts(180, 30), endpoint: 6, fps: 30,
      width: 320, height: 180, audio: false});
    cases.push({...cases[cases.length - 1], name: "multi-input-secondary-speed-overlay", secondarySpeed: true});
    for (const expected of cases) {
      const errors = [];
      try {
        const input = "/workerfs/filter-timing-input.mp4", second = "/workerfs/filter-timing-second.mp4";
        const output = "/filter-timing.mp4", source = fixtures[expected.fixture];
        const files = [{name: input, data: source}];
        const options = {input, output, videoFilter: expected.videoFilter, audioFilter: expected.audioFilter,
          noAudio: !expected.audio, crf: 28, audioBitrateKbps: 128};
        if (expected.duration) Object.assign(options, {startTimeSeconds: expected.sourceStart || 0, durationSeconds: expected.duration});
        if (expected.mode === "multi-input") {
          files.push({name: second, data: fixtures[expected.secondFixture || (expected.audio ? "filterCfr" : expected.fixture)]});
          Object.assign(options, {mode: expected.mode, inputs: [{path: input, kind: "video"},
            {path: second, kind: expected.audio ? "audio" : "video"}], mainInputIndex: 0,
            filterComplex: expected.mixedOverlay
              ? `[1:v]${expected.secondarySpeed ? "setpts=PTS/1.5," : ""}scale=64:48[fg];[0:v][fg]overlay=x=0:y=0:eof_action=repeat,hue=s=0[v]`
              : `[0:v]${expected.videoFilter}[v]` + (expected.audio ? `;[1:a]${expected.audioFilter || "anull"}[a]` : ""),
            videoMap: "[v]", audioMap: expected.audio ? "[a]" : undefined});
        }
        const result = await run(files, api.ffmpegFilterBuilderArgs(options), output, expected);
        assert(result.exitCode === 0, `core exited ${result.exitCode}`);
        if (expected.unknownDuration) {
          assert(result.data.length > 1024, "unknown setpts expression returned an empty output");
          assert((result.logs || []).some(line => line.includes("Video frame duration is unknown for setpts expression")),
            "unknown setpts expression did not warn about unavailable frame duration");
          log(`FILTER_COMPAT_PASS ${expected.name} (accepted with warning; no exact-duration claim)`);
          continue;
        }
        // Decode even a structurally bad file: one failure must not hide the
        // dropped-frame defect or prevent subsequent matrix cells from running.
        try { checkTiming(result.data, expected); } catch (error) { errors.push(error.message); }
        try { checkDecoded(await decode(result.data, expected, fixtures), expected); }
        catch (error) { errors.push(error.message); }
      } catch (error) { errors.push(error.message); }
      if (errors.length) {
        failures.push(`${expected.name}: ${errors.join("; ")}`);
        log(`FILTER_TIMING_FAIL ${failures[failures.length - 1]}`);
      } else log(`FILTER_TIMING_PASS ${expected.name}`);
    }
    if (failures.length) throw new Error(`${failures.length}/${cases.length} Filter Builder timing cases failed:\n${failures.join("\n")}`);
    return cases.length;
  }

  root.FilterBuilderTiming = {run: runFilterBuilderTimingRegressions, pixelSummary, lumaReference, referenceError, assertPreviewAlignment, rejectWrongStart, toneFrequency};
  if (typeof module !== "undefined" && module.exports) module.exports = root.FilterBuilderTiming;
})(typeof globalThis !== "undefined" ? globalThis : this);

/* Minimal demuxer only for these small nonfragmented H.264 test outputs.
 * TimingReaders remains the independent timing oracle. This extractor supplies
 * actual compressed access units to the browser's decoder, not fake frames.
 */
(function (root) {
  "use strict";
  const assert = (value, message) => { if (!value) throw new Error(message); };
  function h264Samples(bytes) {
    const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
    const u32 = at => view.getUint32(at), u16 = at => view.getUint16(at);
    const text = at => String.fromCharCode(...bytes.subarray(at, at + 4));
    function boxes(start, end) {
      const result = [];
      while (start < end) {
        assert(start + 8 <= end, "truncated MP4 box");
        const size = u32(start);
        assert(size >= 8 && start + size <= end, "unsupported MP4 box size");
        result.push({type: text(start + 4), start: start + 8, end: start + size}); start += size;
      }
      return result;
    }
    const children = box => boxes(box.start, box.end);
    const one = (list, type) => { const result = list.find(box => box.type === type); assert(result, `missing ${type}`); return result; };
    const moov = children(one(boxes(0, bytes.length), "moov"));
    let stbl, audio = null;
    for (const trak of moov.filter(box => box.type === "trak")) {
      const mdia = children(one(children(trak), "mdia"));
      const kind = text(one(mdia, "hdlr").start + 8);
      const table = children(one(children(one(mdia, "minf")), "stbl"));
      if (kind === "vide") stbl = table;
      if (kind === "soun") {
        const description = one(table, "stsd");
        assert(u32(description.start + 4) === 1, "expected one audio sample description");
        const entry = one(boxes(description.start + 8, description.end), "mp4a");
        assert(entry.end - entry.start >= 28, "truncated AAC sample entry");
        // Read the encoded rate, not AudioContext's resampled output rate.
        audio = {codec: "aac", sampleRate: u32(entry.start + 24) / 65536};
      }
    }
    assert(stbl, "missing video sample table");
    const stsd = one(stbl, "stsd");
    assert(u32(stsd.start + 4) === 1, "expected one video description");
    const avc1 = one(boxes(stsd.start + 8, stsd.end), "avc1");
    const width = u16(avc1.start + 24), height = u16(avc1.start + 26);
    const avcC = one(boxes(avc1.start + 78, avc1.end), "avcC");
    const description = bytes.slice(avcC.start, avcC.end);
    const codec = "avc1." + Array.from(description.slice(1, 4), byte => byte.toString(16).padStart(2, "0")).join("");
    const sizesBox = one(stbl, "stsz"), size = u32(sizesBox.start + 4), count = u32(sizesBox.start + 8);
    const sizes = Array.from({length: count}, (_, i) => size || u32(sizesBox.start + 12 + 4 * i));
    const offsetBox = stbl.find(box => box.type === "stco" || box.type === "co64");
    assert(offsetBox, "missing chunk offsets");
    const offsets = Array.from({length: u32(offsetBox.start + 4)}, (_, i) => offsetBox.type === "stco"
      ? u32(offsetBox.start + 8 + 4 * i) : Number(view.getBigUint64(offsetBox.start + 8 + 8 * i)));
    const mapBox = one(stbl, "stsc");
    const maps = Array.from({length: u32(mapBox.start + 4)}, (_, i) => ({first: u32(mapBox.start + 8 + 12 * i),
      count: u32(mapBox.start + 12 + 12 * i), description: u32(mapBox.start + 16 + 12 * i)}));
    assert(maps.length && maps[0].first === 1 && maps.every(map => map.count > 0 && map.description === 1), "unsupported sample mapping");
    const sync = stbl.find(box => box.type === "stss");
    const keys = sync ? new Set(Array.from({length: u32(sync.start + 4)}, (_, i) => u32(sync.start + 8 + 4 * i) - 1)) : null;
    const timing = root.TimingReaders.readMp4(bytes).video, samples = [];
    let index = 0, mapping = 0;
    for (let chunk = 0; chunk < offsets.length; chunk++) {
      while (mapping + 1 < maps.length && maps[mapping + 1].first <= chunk + 1) mapping++;
      let offset = offsets[chunk];
      for (let n = 0; n < maps[mapping].count; n++, index++) {
        assert(index < count && sizes[index] > 0 && offset + sizes[index] <= bytes.length, "invalid video sample bounds");
        samples.push({type: !keys || keys.has(index) ? "key" : "delta", timestamp: Math.round(timing.pts[index] * 1e6 / timing.timescale),
          duration: Math.round(timing.durations[index] * 1e6 / timing.timescale), data: bytes.subarray(offset, offset + sizes[index])});
        offset += sizes[index];
      }
    }
    assert(index === count && count === timing.sampleCount, "video sample count disagrees with timing");
    return {config: {codec, codedWidth: width, codedHeight: height, description}, samples, audio};
  }
  const canvas = () => typeof OffscreenCanvas === "function" ? new OffscreenCanvas(32, 18) : Object.assign(document.createElement("canvas"), {width: 32, height: 18});
  async function decodeVideo(bytes, capture = 0) {
    assert(typeof VideoDecoder === "function", "VideoDecoder is required for browser decoded-frame verification");
    const source = h264Samples(bytes);
    const supported = await VideoDecoder.isConfigSupported(source.config);
    assert(supported.supported, "browser H.264 VideoDecoder is unavailable");
    let count = 0, pixels, format, failure, capturedFrame;
    const surface = canvas(), context = surface.getContext("2d", {willReadFrequently: true});
    const decoder = new VideoDecoder({error: error => { failure = error; }, output: frame => {
      try {
        if (count === capture) {
          context.drawImage(frame, 0, 0, 32, 18);
          pixels = root.FilterBuilderTiming.pixelSummary(context.getImageData(0, 0, 32, 18).data);
          format = frame.format;
          capturedFrame = frame.clone();
        }
        count++;
      } catch (error) { failure = error; } finally { frame.close(); }
    }});
    try {
      decoder.configure(source.config);
      for (const sample of source.samples) decoder.decode(new EncodedVideoChunk(sample));
      await decoder.flush();
      if (failure) throw failure;
      assert(pixels && capturedFrame, "browser coded-sample decoder returned no captured frame");
      assert(format === "I420" || format === "NV12", "decoded video is not 8-bit 4:2:0");
      const planeBytes = new Uint8Array(capturedFrame.allocationSize());
      const layout = await capturedFrame.copyTo(planeBytes);
      const luma = root.FilterBuilderTiming.lumaReference(planeBytes, layout[0],
        capturedFrame.visibleRect.width, capturedFrame.visibleRect.height);
      return {decodeKind: "browser coded-sample decoded", codec: "h264", count,
        width: source.config.codedWidth, height: source.config.codedHeight,
        yuv420p: true, encodedAudio: source.audio, ...pixels, alignmentLuma: luma,
        captureTimestamp: capturedFrame.timestamp};
    } finally {
      if (capturedFrame) capturedFrame.close();
      if (decoder.state !== "closed") decoder.close();
    }
  }
  function createBrowserDecoder(fixtures) {
    let reference, wrongStart;
    return async function decode(bytes, expected) {
      const result = await decodeVideo(bytes);
      if (expected.sourceStart === 1) {
        reference ||= decodeVideo(fixtures.filterCfr, 30);
        wrongStart ||= decodeVideo(fixtures.filterCfr, 0);
        assert((await reference).captureTimestamp === 1000000, "source reference is not exactly 1s");
        const referencePixels = (await reference).alignmentLuma;
        result.referenceControlRejected = root.FilterBuilderTiming.rejectWrongStart((await wrongStart).alignmentLuma, referencePixels);
        result.referenceError = root.FilterBuilderTiming.referenceError(result.alignmentLuma, referencePixels);
      }
      if (expected.audio) {
        assert(result.encodedAudio && result.encodedAudio.codec === "aac", "missing AAC sample description");
        const context = new AudioContext({sampleRate: 48000});
        try {
          const buffer = await context.decodeAudioData(bytes.slice().buffer);
          result.audio = {codec: "aac", channels: buffer.numberOfChannels, sampleRate: result.encodedAudio.sampleRate,
            duration: buffer.duration, toneHz: root.FilterBuilderTiming.toneFrequency(buffer.getChannelData(0), buffer.sampleRate)};
        } finally { await context.close(); }
      } else {
        // A video-only output may not silently grow an audio stream.
        assert(!result.encodedAudio, "unexpected AAC sample description");
      }
      return result;
    };
  }
  Object.assign(root.FilterBuilderTiming, {createBrowserDecoder, readBrowserSamples: h264Samples});
})(typeof globalThis !== "undefined" ? globalThis : this);
