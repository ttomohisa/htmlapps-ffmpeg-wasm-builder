/* Actual-core regression cases shared by Node/MEMFS and browser/WORKERFS. */
(function (root) {
  "use strict";
  const assert = (condition, message) => { if (!condition) throw new Error(message); };
  const near = (actual, expected, tolerance, message) =>
    assert(Math.abs(actual - expected) <= tolerance, `${message}: expected ${expected}, got ${actual}`);
  const sum = values => values.reduce((a, b) => a + b, 0);

  async function runTimingRegressions(profile, fixtures, run, api, log = () => {}) {
    const readers = root.TimingReaders;
    const failures = [];
    const cases = [];
    const add = (name, test) => cases.push({ name, test });
    const encode = async (input, options, format) => {
      const output = `/timing.${format}`;
      const args = profile === "video-compressor"
        ? api.videoCompressorArgs({input: "/workerfs/timing-input.mp4", output, speed: "fastest", noAudio: true, ...options})
        : profile === "video-to-gif"
          ? api.videoToGifArgs({input: "/workerfs/timing-input.mp4", output, maxWidth: 64, colors: 64, ...options})
          : api.videoToWebpArgs({input: "/workerfs/timing-input.mp4", output, maxWidth: 64, quality: 70, ...options});
      const result = await run(input, args, output);
      assert(result.exitCode === 0, `core exited ${result.exitCode}`);
      return result.data;
    };
    const checkMp4 = (bytes, pts, endpoint, label, variableRate = false) => {
      const timing = readers.readMp4(bytes).video;
      assert(timing.sampleCount === pts.length, `${label}: expected ${pts.length} samples, got ${timing.sampleCount}`);
      const sorted = timing.pts.slice().sort((a, b) => a - b);
      const start = sorted[0];
      sorted.forEach((ptsTick, index) => near((ptsTick - start) / timing.timescale, pts[index], 1 / timing.timescale, `${label}: presentation timestamp ${index}`));
      assert(timing.durations.every(duration => duration > 0), `${label}: zero-duration terminal sample`);
      // stts measures decode-order intervals. PTS + stts is only a valid
      // sample-end oracle for CFR; reordered VFR endpoints come from elst.
      if (!variableRate) {
        near((timing.presentationEnd - start) / timing.timescale, endpoint, 1 / timing.timescale, `${label}: presentation endpoint`);
        near(timing.duration / timing.timescale, endpoint, 1 / timing.timescale, `${label}: media duration`);
      }
      assert(timing.editPresentationEnd, `${label}: missing video edit-list coverage`);
      if (timing.editPresentationEnd) {
        const editEnd = timing.editPresentationEnd.numerator / timing.editPresentationEnd.denominator;
        near((editEnd - start) / timing.timescale, endpoint, 0.0011, `${label}: edit-list endpoint`);
      }
      return timing;
    };
    if (profile === "video-compressor") {
      for (const fps of [0, 24, 12.5]) add(`mp4-cfr-${fps || "original"}`, async () => {
        const rate = fps || 24;
        const count = Math.round(3 * rate);
        const bytes = await encode(fixtures.cfr, {fps}, "mp4");
        const timing = checkMp4(bytes, Array.from({length: count}, (_, i) => i / rate), count / rate, `CFR ${rate}`);
        timing.durations.forEach(duration => near(duration / timing.timescale, 1 / rate, 1 / timing.timescale, "CFR sample duration"));
      });
      add("mp4-original-vfr", async () => checkMp4(await encode(fixtures.vfr, {fps: 0}, "mp4"), [0, .083, .207, .249, .491, .532], .573, "VFR", true));
      add("mp4-original-single", async () => checkMp4(await encode(fixtures.single, {fps: 0}, "mp4"), [0], .1, "single frame"));
      add("webm-cfr-control", async () => {
        const bytes = await encode(fixtures.cfr, {fps: 24, codec: "vp9"}, "webm");
        const result = await run(bytes, api.videoCompressorInspectArgs({input: "/workerfs/timing-input.mp4", output: "/inspect.json"}), "/inspect.json");
        assert(result.exitCode === 0, "WebM inspection failed");
        const info = JSON.parse(new TextDecoder().decode(result.data));
        assert(info.video.codec === "vp9", "WebM control lost VP9");
        near(info.duration, 3, .0011, "WebM endpoint");
      });
      add("reject-negative-fps", async () => {
        const result = await run(fixtures.cfr, ["--input", "/workerfs/timing-input.mp4", "--output", "/bad.mp4", "--fps", "-1"], "/bad.mp4");
        assert(result.exitCode !== 0, "Negative FPS must fail");
      });
    } else if (profile === "video-to-gif") {
      for (const fps of [10, 15, 30]) add(`gif-${fps}-fps`, async () => {
        const timing = readers.readGif(await encode(fixtures.cfr, {fps, start: 0, end: 3}, "gif"));
        const count = 3 * fps;
        assert(timing.frameCount === count, `Expected ${count} GIF frames, got ${timing.frameCount}`);
        assert(timing.loopCount === 0, "GIF loop metadata changed");
        const expected = Array.from({length: count}, (_, i) => i + 1 < count
          ? Math.round((i + 1) * 100 / fps) - Math.round(i * 100 / fps)
          : Math.round(100 / fps));
        assert(timing.delaysCs.length === count, "Missing GIF graphic control extension");
        timing.delaysCs.forEach((delay, i) => assert(delay === expected[i], `GIF frame ${i}: expected ${expected[i]} cs, got ${delay}`));
        assert(timing.totalDurationCs === sum(expected), "GIF total duration differs from sample intervals");
      });
      add("gif-single-frame", async () => {
        const timing = readers.readGif(await encode(fixtures.single, {fps: 10, start: 0, end: .1}, "gif"));
        assert(timing.frameCount === 1, "Expected a one-frame GIF");
        assert(timing.delaysCs[0] === 10, `Single GIF delay: expected 10 cs, got ${timing.delaysCs[0]}`);
      });
    } else if (profile === "video-to-webp") {
      add("webp-duration-control", async () => {
        const timing = readers.readWebp(await encode(fixtures.cfr, {fps: 10, start: 0, end: 3}, "webp"));
        assert(timing.frameCount > 1 && timing.frameCount <= 30, `Unexpected WebP frame count ${timing.frameCount}`);
        assert(timing.loopCount === 0, "WebP loop metadata changed");
        // libwebp may coalesce identical adjacent frames; their full intervals must remain.
        assert(timing.durationsMs.every(duration => duration > 0 && duration % 100 === 0), "WebP frame duration changed");
        assert(timing.totalDurationMs === 3000, "WebP endpoint changed");
      });
    } else return;
    for (const {name, test} of cases) {
      try { await test(); log(`TIMING_PASS ${name}`); }
      catch (error) { failures.push(`${name}: ${error.message}`); log(`TIMING_FAIL ${failures.at(-1)}`); }
    }
    if (failures.length) throw new Error(failures.join("\n"));
    return cases.length;
  }
  root.runTimingRegressions = runTimingRegressions;
  if (typeof module !== "undefined" && module.exports) module.exports = runTimingRegressions;
})(typeof globalThis !== "undefined" ? globalThis : this);
