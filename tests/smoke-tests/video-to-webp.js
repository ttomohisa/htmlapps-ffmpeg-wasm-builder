const runner = await BrowserFFmpeg.loadEmbedded({ coreJsText, wasmBytes });
const outputPath = "/output.webp";
const result = await runner.run({
  files: [{ name: "/workerfs/input.mp4", data: new Blob([input], { type: "video/mp4" }), workerfs: true }],
  outputs: [outputPath],
  args: BrowserFFmpeg.videoToWebpArgs({
    input: "/workerfs/input.mp4",
    output: outputPath,
    start: 0,
    end: 0.85,
    maxWidth: 96,
    fps: 8,
    quality: 70,
    compressionLevel: 3,
    lossless: false
  }),
  onLog: ({ message }) => append(message)
});
runner.dispose();

if (result.exitCode !== 0) throw new Error("Runner exit code was " + result.exitCode);
if (!result.files || result.files.length !== 1) throw new Error("Expected exactly one WebP output");
const output = result.files[0].data;
if (!(output instanceof Uint8Array)) throw new Error("WebP output is not a Uint8Array");
if (output.byteLength < 512) throw new Error("WebP output is unexpectedly small: " + output.byteLength);
if (String.fromCharCode(...output.slice(0, 4)) !== "RIFF") throw new Error("WebP output does not start with RIFF");
if (String.fromCharCode(...output.slice(8, 12)) !== "WEBP") throw new Error("WebP output does not contain WEBP signature");
if (!containsAscii(output, "ANIM")) throw new Error("WebP output does not contain ANIM chunk");
if (!containsAscii(output, "ANMF")) throw new Error("WebP output does not contain ANMF frames");
pass("webpBytes=" + output.byteLength);
