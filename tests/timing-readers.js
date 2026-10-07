/* Structural timing oracles for small, non-fragmented regression outputs.
 * No decoder guesses: zero durations and clipped edit lists remain visible.
 * MP4 numbers are exact safe integers; larger files/timelines are rejected.
 */
(function (root) {
  'use strict';

  function reader(input, format) {
    const fail = message => { throw new Error(format + ': ' + message); };
    let bytes;
    if (ArrayBuffer.isView(input) && input.BYTES_PER_ELEMENT === 1)
      bytes = new Uint8Array(input.buffer, input.byteOffset, input.byteLength);
    else if (input instanceof ArrayBuffer) bytes = new Uint8Array(input);
    else fail('expected a byte array or ArrayBuffer');
    const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
    const need = (offset, size, end = bytes.length) => {
      if (!Number.isSafeInteger(offset) || !Number.isSafeInteger(size) || offset < 0 || size < 0 || offset + size > end)
        fail('truncated or invalid structure at byte ' + offset);
    };
    const safe = value => {
      if (!Number.isSafeInteger(value)) fail('timing value exceeds safe integer range');
      return value;
    };
    const u8 = offset => { need(offset, 1); return view.getUint8(offset); };
    const u16 = (offset, little = false) => { need(offset, 2); return view.getUint16(offset, little); };
    const i16 = offset => { need(offset, 2); return view.getInt16(offset); };
    const u32 = (offset, little = false) => { need(offset, 4); return view.getUint32(offset, little); };
    const i32 = offset => { need(offset, 4); return view.getInt32(offset); };
    const u64 = offset => { need(offset, 8); return safe(Number(view.getBigUint64(offset))); };
    const i64 = offset => { need(offset, 8); return safe(Number(view.getBigInt64(offset))); };
    const ascii = (offset, count) => {
      need(offset, count);
      let text = '';
      for (let i = 0; i < count; i++) text += String.fromCharCode(bytes[offset + i]);
      return text;
    };
    return {bytes, fail, need, safe, u8, u16, i16, u32, i32, u64, i64, ascii};
  }

  function readMp4(input) {
    const r = reader(input, 'MP4');
    const MAX_SAMPLES = 1000000;
    function boxes(start, end) {
      const result = [];
      while (start < end) {
        r.need(start, 8, end);
        let size = r.u32(start), header = 8;
        const type = r.ascii(start + 4, 4);
        if (size === 1) { r.need(start, 16, end); size = r.u64(start + 8); header = 16; }
        else if (size === 0) size = end - start;
        if (size < header) r.fail('invalid ' + type + ' box size');
        r.need(start, size, end);
        result.push({type, start: start + header, end: start + size});
        start += size;
      }
      return result;
    }
    function one(list, type, required = true) {
      const matches = list.filter(box => box.type === type);
      if (matches.length > 1 || (required && matches.length !== 1)) r.fail('expected one ' + type + ' box');
      return matches[0];
    }
    function children(box) { return boxes(box.start, box.end); }
    function fullVersion(box, allowed = [0]) {
      r.need(box.start, 4, box.end);
      const version = r.u8(box.start);
      if (!allowed.includes(version)) r.fail('unsupported ' + box.type + ' version');
      return version;
    }
    function timeHeader(box) {
      const version = fullVersion(box, [0, 1]);
      const offset = box.start + (version ? 20 : 12);
      r.need(box.start, (box.type === 'mvhd' ? 100 : 24) + (version ? 12 : 0), box.end);
      const timescale = r.u32(offset);
      if (!timescale) r.fail('zero ' + box.type + ' timescale');
      return {timescale, duration: version ? r.u64(offset + 4) : r.u32(offset + 4)};
    }
    function runs(box, signed) {
      fullVersion(box, signed ? [0, 1] : [0]);
      r.need(box.start, 8, box.end);
      const count = r.u32(box.start + 4);
      if (box.end - box.start !== 8 + count * 8) r.fail('invalid ' + box.type + ' entry count');
      const result = [];
      let samples = 0;
      for (let i = 0; i < count; i++) {
        const offset = box.start + 8 + i * 8;
        const n = r.u32(offset);
        if (!n || (samples += n) > MAX_SAMPLES) r.fail(box.type + ' samples exceed test-reader limit or have empty run');
        result.push({count: n, delta: signed && r.u8(box.start) === 1 ? r.i32(offset + 4) : r.u32(offset + 4)});
      }
      return {entries: result, samples};
    }
    const top = boxes(0, r.bytes.length);
    if (top.some(box => box.type === 'moof')) r.fail('fragmented MP4 is unsupported');
    const moov = children(one(top, 'moov'));
    if (moov.some(box => box.type === 'mvex')) r.fail('fragmented MP4 is unsupported');
    const movie = timeHeader(one(moov, 'mvhd'));
    let selected = null;
    for (const track of moov.filter(box => box.type === 'trak')) {
      const trackBoxes = children(track);
      const media = children(one(trackBoxes, 'mdia'));
      const hdlr = one(media, 'hdlr');
      fullVersion(hdlr); r.need(hdlr.start, 24, hdlr.end);
      if (r.ascii(hdlr.start + 8, 4) !== 'vide') continue;
      if (selected) r.fail('multiple video tracks are unsupported');
      selected = {trackBoxes, media};
    }
    if (!selected) r.fail('video track is missing');
    const media = timeHeader(one(selected.media, 'mdhd'));
    const sampleBoxes = children(one(children(one(selected.media, 'minf')), 'stbl'));
    const timing = runs(one(sampleBoxes, 'stts'), false);
    if (!timing.samples) r.fail('video sample table is empty');
    const durations = [], dts = [];
    let decodeDuration = 0;
    for (const entry of timing.entries) {
      for (let i = 0; i < entry.count; i++) {
        dts.push(decodeDuration); durations.push(entry.delta);
        decodeDuration = r.safe(decodeDuration + entry.delta);
      }
    }
    const ctts = one(sampleBoxes, 'ctts', false);
    const ctsOffsets = [];
    if (ctts) {
      const composition = runs(ctts, true);
      if (composition.samples !== timing.samples) r.fail('ctts sample count differs from stts');
      for (const entry of composition.entries)
        for (let i = 0; i < entry.count; i++) ctsOffsets.push(entry.delta);
    } else {
      for (let i = 0; i < timing.samples; i++) ctsOffsets.push(0);
    }
    const pts = dts.map((value, i) => r.safe(value + ctsOffsets[i]));
    let presentationStart = Infinity, presentationEnd = -Infinity;
    for (let i = 0; i < pts.length; i++) {
      presentationStart = Math.min(presentationStart, pts[i]);
      presentationEnd = Math.max(presentationEnd, r.safe(pts[i] + durations[i]));
    }
    const edits = [];
    const edts = one(selected.trackBoxes, 'edts', false);
    if (edts) {
      const elst = one(children(edts), 'elst');
      const version = fullVersion(elst, [0, 1]);
      r.need(elst.start, 8, elst.end);
      const count = r.u32(elst.start + 4), stride = version ? 20 : 12;
      if (elst.end - elst.start !== 8 + count * stride) r.fail('invalid elst entry count');
      for (let i = 0; i < count; i++) {
        const offset = elst.start + 8 + stride * i, rateOffset = offset + (version ? 16 : 8);
        const edit = {
          segmentDuration: version ? r.u64(offset) : r.u32(offset),
          mediaTime: version ? r.i64(offset + 8) : r.i32(offset + 4),
          mediaRateInteger: r.i16(rateOffset), mediaRateFraction: r.i16(rateOffset + 2)
        };
        if (edit.mediaTime < -1 || edit.mediaRateInteger !== 1 || edit.mediaRateFraction !== 0)
          r.fail('unsupported elst media time or rate');
        edits.push(edit);
      }
    }
    // Keep edit coverage rational: movie-timescale rounding must not be hidden.
    let editPresentationEnd = null, endpointNumerator = null;
    const denominator = BigInt(movie.timescale);
    for (const edit of edits) {
      if (edit.mediaTime === -1) continue;
      const numerator = BigInt(edit.mediaTime) * denominator + BigInt(edit.segmentDuration) * BigInt(media.timescale);
      if (endpointNumerator === null || numerator > endpointNumerator) endpointNumerator = numerator;
    }
    if (endpointNumerator !== null) {
      let a = endpointNumerator, b = denominator;
      while (b) { const next = a % b; a = b; b = next; }
      editPresentationEnd = {numerator: r.safe(Number(endpointNumerator / a)), denominator: r.safe(Number(denominator / a))};
    }
    return {movieTimescale: movie.timescale, movieDuration: movie.duration, video: {
      timescale: media.timescale, duration: media.duration, stts: timing.entries,
      sampleCount: timing.samples, durations, dts, ctsOffsets, pts,
      decodeDuration, presentationStart, presentationEnd, edits, editPresentationEnd
    }};
  }

  function readGif(input) {
    const r = reader(input, 'GIF');
    r.need(0, 13);
    if (!['GIF87a', 'GIF89a'].includes(r.ascii(0, 6))) r.fail('invalid signature');
    const width = r.u16(6, true), height = r.u16(8, true);
    if (!width || !height) r.fail('invalid canvas dimensions');
    let offset = 13, pendingDelay = null, loopCount = null, trailer = false;
    const delaysCs = [];
    const take = size => { r.need(offset, size); const start = offset; offset += size; return start; };
    const subblocks = () => {
      const blocks = [];
      for (;;) {
        const size = r.u8(take(1));
        if (!size) return blocks;
        blocks.push({start: take(size), size});
      }
    };
    if (r.u8(10) & 0x80) take(3 * (1 << ((r.u8(10) & 7) + 1)));
    while (offset < r.bytes.length) {
      const kind = r.u8(take(1));
      if (kind === 0x3b) { trailer = true; break; }
      if (kind === 0x21) {
        const label = r.u8(take(1));
        const blocks = subblocks();
        if (label === 0xf9) {
          if (blocks.length !== 1 || blocks[0].size !== 4 || pendingDelay !== null) r.fail('malformed or unpaired GCE');
          pendingDelay = r.u16(blocks[0].start + 1, true);
        } else if (label === 0xff) {
          if (!blocks.length || blocks[0].size !== 11) r.fail('invalid application extension');
          if (['NETSCAPE2.0', 'ANIMEXTS1.0'].includes(r.ascii(blocks[0].start, 11))) {
            if (blocks.length !== 2 || blocks[1].size !== 3 || r.u8(blocks[1].start) !== 1 || loopCount !== null)
              r.fail('invalid or duplicate loop extension');
            loopCount = r.u16(blocks[1].start + 1, true);
          }
        } else if (label === 0x01) {
          if (!blocks.length || blocks[0].size !== 12) r.fail('invalid plain-text extension');
          pendingDelay = null; // A GCE also applies to a following text graphic.
        }
      } else if (kind === 0x2c) {
        const descriptor = take(9);
        if (!r.u16(descriptor + 4, true) || !r.u16(descriptor + 6, true)) r.fail('invalid image dimensions');
        const packed = r.u8(descriptor + 8);
        if (packed & 0x80) take(3 * (1 << ((packed & 7) + 1)));
        const codeSize = r.u8(take(1));
        if (codeSize < 2 || codeSize > 8) r.fail('invalid LZW code size');
        if (!subblocks().length) r.fail('empty image data');
        delaysCs.push(pendingDelay === null ? 0 : pendingDelay);
        pendingDelay = null;
      } else r.fail('unexpected block ' + kind);
    }
    if (!trailer || offset !== r.bytes.length || !delaysCs.length || pendingDelay !== null)
      r.fail('missing trailer, trailing data, or unpaired image control');
    const totalDurationCs = delaysCs.reduce((sum, delay) => r.safe(sum + delay), 0);
    return {width, height, frameCount: delaysCs.length, delaysCs, totalDurationCs, loopCount};
  }

  function readWebp(input) {
    const r = reader(input, 'WebP');
    r.need(0, 12);
    if (r.ascii(0, 4) !== 'RIFF' || r.ascii(8, 4) !== 'WEBP' || r.u32(4, true) + 8 !== r.bytes.length)
      r.fail('invalid RIFF signature or size');
    function chunks(start, end) {
      const result = [];
      while (start < end) {
        r.need(start, 8, end);
        const size = r.u32(start + 4, true), paddedSize = size + (size & 1);
        r.need(start + 8, paddedSize, end);
        if ((size & 1) && r.u8(start + 8 + size) !== 0) r.fail('invalid RIFF padding');
        result.push({type: r.ascii(start, 4), start: start + 8, end: start + 8 + size, size});
        start += 8 + paddedSize;
      }
      return result;
    }
    let loopCount = null, animated = false, staticFrames = 0;
    const durationsMs = [];
    for (const chunk of chunks(12, r.bytes.length)) {
      if (chunk.type === 'ANIM') {
        if (chunk.size !== 6 || animated) r.fail('invalid or duplicate ANIM chunk');
        animated = true; loopCount = r.u16(chunk.start + 4, true);
      } else if (chunk.type === 'ANMF') {
        if (!animated || chunk.size < 16) r.fail('ANMF lacks ANIM or frame header');
        const payload = chunks(chunk.start + 16, chunk.end);
        if (payload.filter(item => item.type === 'VP8 ' || item.type === 'VP8L').length !== 1)
          r.fail('ANMF must contain one image bitstream');
        const offset = chunk.start + 12;
        durationsMs.push(r.u8(offset) + r.u8(offset + 1) * 256 + r.u8(offset + 2) * 65536);
      } else if (chunk.type === 'VP8 ' || chunk.type === 'VP8L') {
        staticFrames++;
      } else if (chunk.type === 'VP8X' && chunk.size !== 10) r.fail('invalid VP8X chunk');
    }
    if (animated ? (!durationsMs.length || staticFrames) : staticFrames !== 1) r.fail('missing or conflicting image frames');
    return {frameCount: animated ? durationsMs.length : 1, durationsMs,
      totalDurationMs: animated ? durationsMs.reduce((sum, delay) => r.safe(sum + delay), 0) : null, loopCount};
  }

  const api = {readMp4, readGif, readWebp};
  root.TimingReaders = api;
  if (typeof module === 'object' && module.exports) module.exports = api;
})(globalThis);
