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
runner.dispose();

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
if (meta.runnerVersion !== "1.3.0") throw new Error("Unexpected runner version: " + meta.runnerVersion);
if (meta.count !== 12 || meta.columns !== 4 || meta.rows !== 3) throw new Error("Unexpected grid metadata");
if (meta.cellWidth !== 160 || meta.cellHeight !== 90) throw new Error("Unexpected cell dimensions");
if (meta.sheetWidth !== 640 || meta.sheetHeight !== 270) throw new Error("Unexpected sheet dimensions");
if (meta.codec !== "h264") throw new Error("Expected H.264 smoke codec, got " + meta.codec);
if (!Array.isArray(meta.samples) || meta.samples.length !== 12) throw new Error("Expected 12 sample timestamps");
for (let i = 1; i < meta.samples.length; i++) {
  if (!(meta.samples[i].targetSeconds >= meta.samples[i - 1].targetSeconds)) throw new Error("Target timestamps are not ordered");
}
pass("samples=" + meta.samples.length + "_size=" + ppm.width + "x" + ppm.height);
