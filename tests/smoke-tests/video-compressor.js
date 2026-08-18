const runner = await BrowserFFmpeg.loadEmbedded({ coreJsText, wasmBytes });
const result = await runner.run({
  files: [{ name: "/input.mp4", data: input }],
  outputs: ["/output.mp4"],
  args: BrowserFFmpeg.videoCompressorArgs({
    input: "/input.mp4",
    output: "/output.mp4",
    maxWidth: 96,
    crf: 35,
    preset: "ultrafast",
    audioBitrateKbps: 32
  }),
  onLog: ({ message }) => append(message)
});
runner.dispose();

if (result.exitCode !== 0) throw new Error("Runner exit code was " + result.exitCode);
if (!result.files || result.files.length !== 1) throw new Error("Expected exactly one output file");
const output = result.files[0].data;
if (!(output instanceof Uint8Array)) throw new Error("Output is not a Uint8Array");
if (output.byteLength < 1024) throw new Error("Output MP4 is unexpectedly small: " + output.byteLength);
if (String.fromCharCode(...output.slice(4, 8)) !== "ftyp") throw new Error("Output does not contain an MP4 ftyp box at offset 4");
if (!containsAscii(output, "moov")) throw new Error("Output MP4 does not contain a moov box");
if (!containsAscii(output, "mdat")) throw new Error("Output MP4 does not contain an mdat box");
if (!containsAscii(output, "avc1")) throw new Error("Output MP4 does not advertise H.264/avc1 video");
if (!containsAscii(output, "mp4a")) throw new Error("Output MP4 does not advertise AAC/mp4a audio");
pass("bytes=" + output.byteLength);
