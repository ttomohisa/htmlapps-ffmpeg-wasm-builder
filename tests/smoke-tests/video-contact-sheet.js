const runner = await BrowserFFmpeg.loadEmbedded({ coreJsText, wasmBytes });
const ppmPath = "/contact-sheet.ppm";
const jsonPath = "/contact-sheet.json";
const result = await runner.run({
  files: [{ name: "/workerfs/input.mp4", data: new Blob([input], { type: "video/mp4" }), workerfs: true }],
  outputs: [ppmPath, jsonPath],
  args: BrowserFFmpeg.videoContactSheetArgs({
    input: "/workerfs/input.mp4",
    output: ppmPath,
    metadataOutput: jsonPath,
    count: 12,
    thumbSize: 160
  }),
  onLog: ({ message }) => append(message)
});

if (result.exitCode !== 0) throw new Error("Runner exit code was " + result.exitCode);
if (!result.files || result.files.length !== 2) throw new Error("Expected PPM + JSON outputs");
const ppm = BrowserFFmpeg.decodePpmOutput(result, ppmPath);
const meta = BrowserFFmpeg.decodeJsonOutput(result, jsonPath);

if (ppm.width !== 640 || ppm.height !== 270) throw new Error(`Unexpected PPM dimensions: ${ppm.width}x${ppm.height}`);
if (ppm.pixels.byteLength !== ppm.width * ppm.height * 3) throw new Error("PPM RGB byte count is incorrect");
let min = 255, max = 0;
for (let i = 0; i < ppm.pixels.length; i += 97) { min = Math.min(min, ppm.pixels[i]); max = Math.max(max, ppm.pixels[i]); }
if (max <= min) throw new Error("Contact sheet appears to contain no image variation");
if (meta.schemaVersion !== 1) throw new Error("Unexpected metadata schema: " + meta.schemaVersion);
if (meta.runnerVersion !== "1.5.0") throw new Error("Unexpected runner version: " + meta.runnerVersion);
if (meta.count !== 12 || meta.columns !== 4 || meta.rows !== 3) throw new Error("Unexpected grid metadata");
if (meta.cellWidth !== 160 || meta.cellHeight !== 90) throw new Error("Unexpected cell dimensions");
if (meta.sheetWidth !== 640 || meta.sheetHeight !== 270) throw new Error("Unexpected sheet dimensions");
if (meta.codec !== "h264") throw new Error("Expected H.264 smoke codec, got " + meta.codec);
if (!Array.isArray(meta.samples) || meta.samples.length !== 12) throw new Error("Expected 12 sample timestamps");
for (let i = 1; i < meta.samples.length; i++) {
  if (!(meta.samples[i].targetSeconds >= meta.samples[i - 1].targetSeconds)) throw new Error("Target timestamps are not ordered");
}
// The same global targets must produce byte-identical output in either mode.
const sharedInput = new Blob([input], { type: "video/mp4" });
async function extract(count, partitionIndex) {
  const options = { input:"/workerfs/input.mp4", output:ppmPath, metadataOutput:jsonPath, count, thumbSize:160 };
  if (partitionIndex !== undefined) Object.assign(options, {partitionCount:2, partitionIndex});
  const progress = [];
  const output = await runner.run({
    files:[{name:"/workerfs/input.mp4", data:sharedInput, workerfs:true}],
    outputs:[ppmPath,jsonPath], args:BrowserFFmpeg.videoContactSheetArgs(options),
    onProgress:value => progress.push(value)
  });
  if (progress.at(-1) !== 1) throw new Error("Partition progress did not complete");
  return {ppm:BrowserFFmpeg.decodePpmOutput(output,ppmPath), meta:BrowserFFmpeg.decodeJsonOutput(output,jsonPath)};
}
for (const count of [12,24,48]) {
  const full = count === 12 ? {ppm,meta} : await extract(count);
  const parts = await Promise.all([extract(count,0),extract(count,1)]);
  const merged = new Uint8Array(full.ppm.pixels.length);
  const samples = [];
  const visited = new Set();
  for (const [partitionIndex, part] of parts.entries()) {
    if (part.meta.partitionCount !== 2 || part.meta.partitionIndex !== partitionIndex)
      throw new Error("Missing partition identity");
    for (const key of ["schemaVersion","runnerVersion","format","codec","durationSeconds","count","columns","rows","rotationDegrees","cellWidth","cellHeight","sheetWidth","sheetHeight"])
      if (part.meta[key] !== full.meta[key]) throw new Error("Partition metadata differs: " + key);
    if (part.ppm.width !== full.ppm.width || part.ppm.height !== full.ppm.height)
      throw new Error("Partition PPM geometry differs");
    if (part.meta.samples.length !== count/2) throw new Error("Unexpected partition sample count");
    for (const sample of part.meta.samples) {
      if (sample.index % 2 !== partitionIndex || visited.has(sample.index))
        throw new Error("Partition sample ownership overlaps or is incorrect");
      visited.add(sample.index); samples[sample.index] = sample;
      const x = (sample.index % full.meta.columns) * full.meta.cellWidth;
      const y = Math.floor(sample.index / full.meta.columns) * full.meta.cellHeight;
      for (let row=0;row<full.meta.cellHeight;row++) {
        const start = ((y+row)*full.ppm.width+x)*3;
        merged.set(part.ppm.pixels.subarray(start,start+full.meta.cellWidth*3),start);
      }
    }
    for (let index=0;index<count;index++) {
      if (index%2 === partitionIndex) continue;
      const x=(index%full.meta.columns)*full.meta.cellWidth;
      const y=Math.floor(index/full.meta.columns)*full.meta.cellHeight;
      for (let row=0;row<full.meta.cellHeight;row++) {
        const start=((y+row)*full.ppm.width+x)*3;
        for (let byte=start;byte<start+full.meta.cellWidth*3;byte++)
          if (part.ppm.pixels[byte] !== 0) throw new Error("Unowned tile was not black");
      }
    }
  }
  if (visited.size !== count || JSON.stringify(samples) !== JSON.stringify(full.meta.samples))
    throw new Error("Merged samples/timestamps differ for count " + count);
  for (let byte=0;byte<merged.length;byte++)
    if (merged[byte] !== full.ppm.pixels[byte]) throw new Error("Merged RGB byte differs at " + byte + " for count " + count);
  append("Partition parity: " + count + " samples, every RGB byte and timestamp matched");
}
runner.dispose();
pass("samples=" + meta.samples.length + "_size=" + ppm.width + "x" + ppm.height + "_partition_parity=12-24-48");
