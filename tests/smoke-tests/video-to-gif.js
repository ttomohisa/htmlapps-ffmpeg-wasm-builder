const runner = await BrowserFFmpeg.loadEmbedded({ coreJsText, wasmBytes });
const outputPath = "/output.gif";
const result = await runner.run({
  files: [{ name: "/workerfs/input.mp4", data: new Blob([input], { type: "video/mp4" }), workerfs: true }],
  outputs: [outputPath],
  args: BrowserFFmpeg.videoToGifArgs({
    input: "/workerfs/input.mp4",
    output: outputPath,
    start: 0,
    end: 0.85,
    maxWidth: 96,
    fps: 8,
    colors: 64,
    dither: "sierra2_4a"
  }),
  onLog: ({ message }) => append(message)
});
runner.dispose();

if (result.exitCode !== 0) throw new Error("Runner exit code was " + result.exitCode);
if (!result.files || result.files.length !== 1) throw new Error("Expected exactly one GIF output");
const output = result.files[0].data;
if (!(output instanceof Uint8Array)) throw new Error("GIF output is not a Uint8Array");
if (output.byteLength < 512) throw new Error("GIF output is unexpectedly small: " + output.byteLength);
const header = String.fromCharCode(...output.slice(0, 6));
if (header !== "GIF89a" && header !== "GIF87a") throw new Error("Invalid GIF header: " + header);
if (!containsAscii(output, "NETSCAPE2.0")) throw new Error("GIF does not contain an animation loop extension");
pass("gifBytes=" + output.byteLength);
