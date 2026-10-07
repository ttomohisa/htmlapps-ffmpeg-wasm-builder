'use strict';
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const test = require('node:test');
const readerPath = path.join(__dirname, 'timing-readers.js');
const readers = fs.existsSync(readerPath) ? require(readerPath) : {};
const u32 = value => { const b = Buffer.alloc(4); b.writeUInt32BE(value); return b; };
const i32 = value => { const b = Buffer.alloc(4); b.writeInt32BE(value); return b; };
const u64 = value => { const b = Buffer.alloc(8); b.writeBigUInt64BE(BigInt(value)); return b; };
const i64 = value => { const b = Buffer.alloc(8); b.writeBigInt64BE(BigInt(value)); return b; };
const box = (kind, ...body) => { const b = Buffer.concat(body); return Buffer.concat([u32(b.length + 8), Buffer.from(kind), b]); };
const full = (version, ...body) => Buffer.concat([Buffer.from([version, 0, 0, 0]), ...body]);
const stts = entries => box('stts', full(0, u32(entries.length), ...entries.flatMap(([count, delta]) => [u32(count), u32(delta)])));
function movie({ durations = [[3, 40]], offsets = null, cttsVersion = 0, edits = [[120, 80]], version = 0, handler = 'vide', mediaDuration = 120, movieDuration = 120, trailing = [] } = {}) {
  const time = (scale, duration, rest) => full(version, version ? Buffer.alloc(16) : Buffer.alloc(8), u32(scale), version ? u64(duration) : u32(duration), Buffer.alloc(rest));
  const mdhd = box('mdhd', time(1000, mediaDuration, 4));
  const hdlr = box('hdlr', full(0, Buffer.alloc(4), Buffer.from(handler), Buffer.alloc(12), Buffer.from([0])));
  const ctts = offsets ? box('ctts', full(cttsVersion, u32(offsets.length), ...offsets.flatMap(([count, offset]) => [u32(count), cttsVersion ? i32(offset) : u32(offset)]))) : Buffer.alloc(0);
  const elst = edits === null ? Buffer.alloc(0) : box('edts', box('elst', full(version, u32(edits.length), ...edits.flatMap(([duration, time]) => [version ? u64(duration) : u32(duration), version ? i64(time) : i32(time), Buffer.from([0, 1, 0, 0])]))));
  return box('moov', box('mvhd', time(1000, movieDuration, 80)), box('trak', elst, box('mdia', mdhd, hdlr, box('minf', box('stbl', stts(durations), ctts)))), ...trailing);
}
const gifHeader = Buffer.from('47494638396101000100800000000000ffffff', 'hex');
const gce = delay => Buffer.from([0x21, 0xf9, 4, 0, delay & 255, delay >> 8, 0, 0]);
const gifImage = Buffer.from('2c0000000001000100000202440100', 'hex');
const gifLoop = Buffer.from('21ff0b4e45545343415045322e300301000000', 'hex');
const gif = (delays, loop = true) => Buffer.concat([gifHeader, ...(loop ? [gifLoop] : []), ...delays.flatMap(delay => [gce(delay), gifImage]), Buffer.from([0x3b])]);
const le32 = value => { const b = Buffer.alloc(4); b.writeUInt32LE(value); return b; };
const chunk = (kind, body) => Buffer.concat([Buffer.from(kind), le32(body.length), body, ...(body.length % 2 ? [Buffer.from([0])] : [])]);
const riff = (...chunks) => { const body = Buffer.concat([Buffer.from('WEBP'), ...chunks]); return Buffer.concat([Buffer.from('RIFF'), le32(body.length), body]); };
const anmf = delay => { const b = Buffer.alloc(16); b.writeUIntLE(delay, 12, 3); return chunk('ANMF', Buffer.concat([b, chunk('VP8 ', Buffer.from([1, 2, 3]))])); };
const webp = delays => riff(chunk('VP8X', Buffer.from([2, 0, 0, 0, 0, 0, 0, 0, 0, 0])), chunk('ANIM', Buffer.alloc(6)), ...delays.map(anmf));

test('MP4 retains B-frame composition offsets, exact endpoints, and edits', () => {
  assert.equal(typeof readers.readMp4, 'function');
  const result = readers.readMp4(movie({offsets: [[1, 80], [1, 120], [1, 40]]}));
  assert.equal(result.movieTimescale, 1000);
  assert.equal(result.movieDuration, 120);
  assert.deepEqual(result.video.stts, [{count: 3, delta: 40}]);
  assert.deepEqual(result.video.dts, [0, 40, 80]);
  assert.deepEqual(result.video.ctsOffsets, [80, 120, 40]);
  assert.deepEqual(result.video.pts, [80, 160, 120]);
  assert.deepEqual(result.video.durations, [40, 40, 40]);
  assert.equal(result.video.sampleCount, 3);
  assert.equal(result.video.duration, 120);
  assert.equal(result.video.decodeDuration, 120);
  assert.equal(result.video.presentationStart, 80);
  assert.equal(result.video.presentationEnd, 200);
  assert.deepEqual(result.video.editPresentationEnd, {numerator: 200, denominator: 1});
});

test('MP4 exposes zero terminal sample and shortened edit without inferring repairs', () => {
  const v = readers.readMp4(movie({durations: [[2, 40], [1, 0]], offsets: [[1, 80], [1, 120], [1, 40]], edits: [[80, 80]], mediaDuration: 80, movieDuration: 80})).video;
  assert.deepEqual(v.durations, [40, 40, 0]);
  assert.equal(v.presentationEnd, 200);
  assert.equal(v.decodeDuration, 80);
  assert.deepEqual(v.editPresentationEnd, {numerator: 160, denominator: 1});
});

test('MP4 supports signed ctts and 64-bit version-one headers and edits', () => {
  const v = readers.readMp4(movie({version: 1, cttsVersion: 1, offsets: [[1, -40], [2, 0]], edits: [[20, -1], [120, 0]]})).video;
  assert.deepEqual(v.pts, [-40, 40, 80]);
  assert.equal(v.presentationStart, -40);
  assert.equal(v.edits[0].mediaTime, -1);
  assert.deepEqual(v.editPresentationEnd, {numerator: 120, denominator: 1});
});

test('MP4 preserves VFR durations and ignores non-video tracks', () => {
  const audio = movie({handler: 'soun'}).subarray(8);
  const video = movie({durations: [[1, 80], [1, 120], [1, 40], [1, 240], [2, 40]], edits: null, mediaDuration: 560}).subarray(8);
  // Keep a single mvhd and append just the audio track before the video track.
  const headerSize = audio.readUInt32BE(0);
  const result = readers.readMp4(box('moov', video.subarray(0, headerSize), audio.subarray(headerSize), video.subarray(headerSize)));
  assert.deepEqual(result.video.pts, [0, 80, 200, 240, 480, 520]);
  assert.equal(result.video.presentationEnd, 560);
  assert.equal(result.video.editPresentationEnd, null);
});

test('MP4 supports extended-size boxes and zero-size final boxes', () => {
  const payload = movie({edits: null}).subarray(8);
  const extended = Buffer.concat([u32(1), Buffer.from('moov'), u64(payload.length + 16), payload]);
  assert.equal(readers.readMp4(extended).video.sampleCount, 3);
  const toEnd = Buffer.concat([u32(0), Buffer.from('moov'), payload]);
  assert.equal(readers.readMp4(toEnd).video.sampleCount, 3);
});

test('MP4 rejects truncation, sample-table mismatch, no video, and unsafe integers', () => {
  const input = movie();
  for (const n of [0, 1, 7, 8, input.length - 1]) assert.throws(() => readers.readMp4(input.subarray(0, n)), /MP4/);
  assert.throws(() => readers.readMp4(movie({offsets: [[2, 0]]})), /MP4.*ctts/);
  assert.throws(() => readers.readMp4(movie({handler: 'soun'})), /MP4.*video/);
  assert.throws(() => readers.readMp4(movie({version: 1, mediaDuration: 9007199254740992n})), /MP4.*safe|MP4.*integer/);
  const bad = Buffer.from(input); bad.writeUInt32BE(7, 0);
  assert.throws(() => readers.readMp4(bad), /MP4/);
  const huge = movie({durations: [[0xffffffff, 1]]});
  assert.throws(() => readers.readMp4(huge), /MP4.*samples|MP4.*limit/);
});

test('GIF follows structural blocks and associates each GCE with its image', () => {
  assert.equal(typeof readers.readGif, 'function');
  assert.deepEqual(readers.readGif(gif([10, 10, 1])), {width: 1, height: 1, frameCount: 3, delaysCs: [10, 10, 1], totalDurationCs: 21, loopCount: 0});
  assert.equal(readers.readGif(gif([7], false)).loopCount, null);
  const noGce = Buffer.concat([gifHeader, gifImage, Buffer.from([0x3b])]);
  assert.deepEqual(readers.readGif(noGce).delaysCs, [0]);
});

test('GIF skips local palettes and opaque compressed-data bytes', () => {
  const image = Buffer.from(gifImage); image[9] = 0x80;
  const palette = Buffer.from([0x21, 0xf9, 4, 10, 0, 0]);
  const withPalette = Buffer.concat([image.subarray(0, 10), palette, image.subarray(10)]);
  const output = Buffer.concat([gifHeader, gce(13), withPalette, Buffer.from([0x3b])]);
  assert.deepEqual(readers.readGif(output).delaysCs, [13]);
});

test('GIF rejects missing trailer, truncated subblocks, malformed GCEs and unpaired controls', () => {
  const input = gif([10]);
  for (const n of [0, 6, 12, input.length - 1, input.length - 3]) assert.throws(() => readers.readGif(input.subarray(0, n)), /GIF/);
  assert.throws(() => readers.readGif(Buffer.concat([input, Buffer.from([0])])), /GIF/);
  assert.throws(() => readers.readGif(Buffer.concat([gifHeader, gce(1), Buffer.from([0x3b])])), /GIF/);
  const bad = Buffer.from(gif([10], false)); bad[21] = 3;
  assert.throws(() => readers.readGif(bad), /GIF/);
});

test('WebP reads little-endian ANMF durations, including odd padded chunks', () => {
  assert.equal(typeof readers.readWebp, 'function');
  assert.deepEqual(readers.readWebp(webp([100, 100, 100])), {frameCount: 3, durationsMs: [100, 100, 100], totalDurationMs: 300, loopCount: 0});
  const staticImage = riff(chunk('VP8 ', Buffer.from([1, 2, 3])));
  assert.deepEqual(readers.readWebp(staticImage), {frameCount: 1, durationsMs: [], totalDurationMs: null, loopCount: null});
});

test('WebP rejects truncation, invalid chunk size, missing ANIM and incomplete frames', () => {
  const input = webp([100]);
  for (const n of [0, 11, input.length - 1, input.length - 3]) assert.throws(() => readers.readWebp(input.subarray(0, n)), /WebP/);
  assert.throws(() => readers.readWebp(Buffer.concat([input, Buffer.from([0])])), /WebP/);
  assert.throws(() => readers.readWebp(riff(anmf(100))), /WebP/);
  assert.throws(() => readers.readWebp(riff(chunk('ANIM', Buffer.alloc(6)), chunk('ANMF', Buffer.alloc(15)))), /WebP/);
  const bad = Buffer.from(input); bad.writeUInt32LE(0xffffffff, 16);
  assert.throws(() => readers.readWebp(bad), /WebP/);
});

test('readers expose the same browser global API without Node dependencies', () => {
  assert.ok(fs.existsSync(readerPath), 'Reader implementation must exist');
  const context = {Uint8Array, ArrayBuffer, DataView};
  vm.createContext(context); vm.runInContext(fs.readFileSync(readerPath, 'utf8'), context);
  assert.equal(typeof context.TimingReaders.readMp4, 'function');
  assert.equal(context.TimingReaders.readMp4(movie()).video.sampleCount, 3);
  assert.equal(context.TimingReaders.readGif(gif([10])).totalDurationCs, 10);
  assert.equal(context.TimingReaders.readWebp(webp([100])).totalDurationMs, 100);
});

test('committed smoke fixture has exact 48-frame, two-second video timing', () => {
  const result = readers.readMp4(fs.readFileSync(path.join(__dirname, 'fixtures/smoke-rotated.mp4')));
  assert.equal(result.video.sampleCount, 48);
  assert.equal(result.video.duration / result.video.timescale, 2);
  assert.ok(result.video.durations.every(duration => duration > 0));
});

test('MP4 retains rational edit endpoints without movie-timescale rounding', () => {
  const input = movie({edits: [[1, 80]]});
  input.writeUInt32BE(3, input.indexOf(Buffer.from('mvhd')) + 16);
  const endpoint = readers.readMp4(input).video.editPresentationEnd;
  assert.deepEqual(endpoint, {numerator: 1240, denominator: 3});
});

test('MP4 rejects unsupported edit rates and fragmented streams', () => {
  const input = movie();
  input.writeUInt16BE(2, input.indexOf(Buffer.from('elst')) + 20);
  assert.throws(() => readers.readMp4(input), /MP4.*rate/);
  assert.throws(() => readers.readMp4(Buffer.concat([movie(), box('moof')])), /MP4.*fragmented/);
  assert.throws(() => readers.readMp4(movie({trailing: [box('mvex')]})), /MP4.*fragmented/);
});

test('all structural readers reject every truncated prefix of their synthetic fixture', () => {
  for (const [read, input, prefix] of [[readers.readMp4, movie(), /MP4/], [readers.readGif, gif([7, 6, 7]), /GIF/], [readers.readWebp, webp([33, 34, 33]), /WebP/]]) {
    for (let size = 0; size < input.length; size++) assert.throws(() => read(input.subarray(0, size)), prefix, 'prefix length ' + size);
  }
});

test('byte-array views honor offsets and ArrayBuffer inputs work', () => {
  for (const [read, input, key] of [[readers.readGif, gif([10]), 'totalDurationCs'], [readers.readWebp, webp([100]), 'totalDurationMs']]) {
    const padded = Buffer.concat([Buffer.alloc(17), input, Buffer.alloc(13)]);
    assert.equal(read(padded.subarray(17, -13))[key], read(input)[key]);
    assert.equal(read(Uint8Array.from(input).buffer)[key], read(input)[key]);
  }
  assert.throws(() => readers.readMp4('not bytes'), /MP4/);
});

test('GIF plain-text graphics consume the preceding GCE without leaking delay to next image', () => {
  const text = Buffer.concat([Buffer.from([0x21, 0x01, 12]), Buffer.alloc(12), Buffer.from([0])]);
  const output = Buffer.concat([gifHeader, gce(10), text, gifImage, Buffer.from([0x3b])]);
  assert.deepEqual(readers.readGif(output).delaysCs, [0]);
});

test('WebP rejects a chunk that overlaps its parent ANMF boundary', () => {
  const body = Buffer.concat([Buffer.alloc(16), Buffer.from('VP8 '), le32(20), Buffer.from([1, 2])]);
  assert.throws(() => readers.readWebp(riff(chunk('ANIM', Buffer.alloc(6)), chunk('ANMF', body))), /WebP/);
});
