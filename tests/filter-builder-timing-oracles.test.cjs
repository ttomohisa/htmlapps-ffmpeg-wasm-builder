// Guard the browser-specific oracles without requiring a browser or native FFmpeg.
const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
globalThis.TimingReaders = require('./timing-readers.js');
const timing = require('./filter-builder-timing-regressions.js');
const fixture = name => fs.readFileSync(path.join(__dirname, 'fixtures', name));

test('preview alignment accepts calibrated correct frames and rejects wrong-start errors', () => {
  assert.doesNotThrow(() => timing.assertPreviewAlignment(.526));
  assert.doesNotThrow(() => timing.assertPreviewAlignment(1.508));
  for (const error of [4, 8.50, 9.168, NaN]) assert.throws(() => timing.assertPreviewAlignment(error), /source 1s/);
  assert.equal(timing.rejectWrongStart([10, 10], [0, 0]), true);
  assert.throws(() => timing.rejectWrongStart([1, 1], [0, 0]), /negative control falsely passed/);
});

test('browser source reference uses weighted luminance instead of RGB arithmetic mean', () => {
  const summary = timing.pixelSummary(Uint8Array.from([255, 0, 0, 255]));
  assert.ok(Math.abs(summary.gray[0] - 76.245) < 1e-9);
});

test('browser demux exposes actual H.264 samples and encoded AAC rate', () => {
  const bytes = fixture('timing-filter-cfr.mp4');
  const actual = timing.readBrowserSamples(bytes);
  assert.equal(actual.samples.length, 180);
  assert.equal(actual.config.codedWidth, 320);
  assert.equal(actual.config.codedHeight, 180);
  assert.equal(actual.audio.sampleRate, 48000);
  assert.equal(actual.audio.codec, 'aac');
  assert.equal(actual.samples[0].type, 'key');
  assert.ok(actual.samples.every(sample => sample.data.length > 0));
  // A forced 48kHz AudioContext would hide this sample-entry mutation.
  const wrongRate = Buffer.from(bytes);
  const type = wrongRate.indexOf(Buffer.from('mp4a'));
  assert.ok(type > 0);
  wrongRate.writeUInt32BE(44100 * 65536, type + 28);
  assert.equal(timing.readBrowserSamples(wrongRate).audio.sampleRate, 44100);
});

test('video-only VFR browser demux retains all samples with no audio', () => {
  const actual = timing.readBrowserSamples(fixture('timing-vfr.mp4'));
  assert.equal(actual.audio, null);
  assert.equal(actual.samples.length, 6);
  assert.deepEqual(actual.samples.map(sample => sample.timestamp), [0, 83000, 207000, 249000, 491000, 532000]);
  assert.equal(actual.samples.at(-1).duration, 41000);
});

// Preview alignment compares decoded luma directly. Browser RGB conversion can
// select a different YUV matrix for untagged source and filtered output.
test('decoded luma reference respects plane offset, stride and spatial averaging', () => {
  const bytes = Uint8Array.from([99, 10, 20, 30, 40, 99, 99, 50, 60, 70, 80, 99]);
  assert.deepEqual(timing.lumaReference(bytes, {offset: 1, stride: 6}, 4, 2, 2, 1), [35, 55]);
  assert.throws(() => timing.lumaReference(bytes.subarray(0, 9), {offset: 1, stride: 6}, 4, 2, 2, 1), /luma plane/);
});
