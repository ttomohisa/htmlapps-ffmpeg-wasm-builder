/* Browser runtime for the single-thread public-libav runner. No SharedArrayBuffer required. */
(() => {
  "use strict";
  const PROGRESS_PREFIX = "__FFMPEG_WASM_PROGRESS__";

  const WORKER_BODY = String.raw`
    (() => {
    "use strict";
    const PROGRESS_PREFIX = "__FFMPEG_WASM_PROGRESS__";
    const ensureParent = (FS, path) => {
      const index = path.lastIndexOf("/");
      if (index <= 0) return;
      try { FS.mkdirTree(path.slice(0, index)); } catch (_) {}
    };
    const splitVirtualPath = (path) => {
      if (typeof path !== "string" || !path.startsWith("/")) throw new Error("Virtual filesystem paths must be absolute: " + path);
      const index = path.lastIndexOf("/");
      if (index <= 0 || index === path.length - 1) throw new Error("WORKERFS inputs must be placed inside a directory, for example /workerfs/input.mp4.");
      return { parent: path.slice(0, index), base: path.slice(index + 1) };
    };
    const mountWorkerFiles = (core, files) => {
      const mounts = new Map();
      for (const file of files) {
        const parts = splitVirtualPath(file.name);
        let entries = mounts.get(parts.parent);
        if (!entries) { entries = []; mounts.set(parts.parent, entries); }
        entries.push({ name: parts.base, data: file.data });
      }
      if (mounts.size === 0) return;
      if (!core.WORKERFS) throw new Error("This FFmpeg WASM profile was not built with WORKERFS support.");
      for (const [mountPoint, blobs] of mounts) {
        try { core.FS.mkdirTree(mountPoint); } catch (_) {}
        core.FS.mount(core.WORKERFS, { blobs }, mountPoint);
      }
    };
    self.onmessage = async (event) => {
      const { wasmBytes, args, files, outputs } = event.data;
      const sendLine = (stream, value) => {
        const message = String(value);
        if (message.startsWith(PROGRESS_PREFIX)) {
          const progress = Number(message.slice(PROGRESS_PREFIX.length).trim());
          if (Number.isFinite(progress)) self.postMessage({ type: "progress", progress: Math.max(0, Math.min(1, progress)) });
          return;
        }
        self.postMessage({ type: "log", stream, message });
      };
      try {
        if (typeof createFFmpegCore !== "function") throw new Error("createFFmpegCore factory was not found.");
        const wasmView = new Uint8Array(wasmBytes);
        const core = await createFFmpegCore({
          // Emscripten's generated loader normally resolves ffmpeg.wasm
          // relative to the JS script.  Our JS runs inside a blob Worker, where
          // that relative URL is invalid.  Instantiate directly from the bytes
          // already transferred to this Worker so hosted and file:// single-HTML
          // execution never depends on URL resolution for the Wasm binary.
          wasmBinary: wasmView,
          instantiateWasm: (imports, successCallback) => {
            const module = new WebAssembly.Module(wasmView);
            const instance = new WebAssembly.Instance(module, imports);
            successCallback(instance, module);
            return instance.exports;
          },
          locateFile: (path, prefix) => prefix + path,
          print: (message) => sendLine("stdout", message),
          printErr: (message) => sendLine("stderr", message)
        });
        const workerFsFiles = [];
        for (const file of files) {
          if (file.workerfs) {
            workerFsFiles.push(file);
            continue;
          }
          ensureParent(core.FS, file.name);
          core.FS.writeFile(file.name, new Uint8Array(file.data));
        }
        mountWorkerFiles(core, workerFsFiles);
        let exitCode = 0;
        try {
          const result = core.callMain(args);
          if (typeof result === "number") exitCode = result;
        } catch (error) {
          if (typeof error?.status === "number" && error.status === 0) exitCode = 0;
          else throw error;
        }
        if (exitCode !== 0) throw new Error("FFmpeg WASM runner exited with code " + exitCode);
        const resultFiles = [];
        const transfer = [];
        for (const name of outputs) {
          const bytes = core.FS.readFile(name);
          const copy = bytes.buffer.slice(bytes.byteOffset, bytes.byteOffset + bytes.byteLength);
          resultFiles.push({ name, data: copy });
          transfer.push(copy);
        }
        self.postMessage({ type: "done", exitCode, files: resultFiles }, transfer);
      } catch (error) {
        self.postMessage({ type: "error", name: error?.name || "Error", message: error?.message || String(error), stack: error?.stack || "" });
      }
    };
    })();
  `;

  const toArrayBuffer = async (value) => {
    if (value instanceof ArrayBuffer) return value.slice(0);
    if (ArrayBuffer.isView(value)) return value.buffer.slice(value.byteOffset, value.byteOffset + value.byteLength);
    if (value instanceof Blob) return await value.arrayBuffer();
    throw new TypeError("Input data must be Blob, File, ArrayBuffer, or TypedArray.");
  };

  const isSupported = () => typeof Worker === "function" && typeof WebAssembly === "object" && typeof Blob === "function" && typeof URL?.createObjectURL === "function";
  const assertSupported = () => { if (!isSupported()) throw new Error("This browser does not support the FFmpeg WASM runtime."); };

  const createWorker = (coreJsText) => {
    // Keep the generated Emscripten glue and our Worker body in one Blob.
    // This avoids nested blob-script loading on file:// pages, where nested
    // blob URL loading is not portable across browsers.
    const url = URL.createObjectURL(new Blob([coreJsText, "\n;", WORKER_BODY], { type: "text/javascript;charset=utf-8" }));
    try {
      return { worker: new Worker(url), url };
    } catch (error) {
      URL.revokeObjectURL(url);
      throw error;
    }
  };

  class FFmpegRunner {
    constructor(coreJsText, wasmBytes, revokeUrls = []) {
      this.coreJsText = coreJsText;
      this.wasmBytes = wasmBytes;
      this.revokeUrls = revokeUrls;
      this.disposed = false;
    }
    async run(options = {}) {
      if (this.disposed) throw new Error("Runner has been disposed.");
      assertSupported();
      const args = Array.isArray(options.args) ? [...options.args] : [];
      const outputs = Array.isArray(options.outputs) ? [...options.outputs] : [];
      const onLog = typeof options.onLog === "function" ? options.onLog : () => {};
      const onProgress = typeof options.onProgress === "function" ? options.onProgress : () => {};
      const files = [];
      const transfer = [];
      for (const file of Array.isArray(options.files) ? options.files : []) {
        if (!file?.name) throw new Error("Every input file needs a virtual filesystem name.");
        if (file.workerfs === true) {
          if (!(file.data instanceof Blob)) throw new TypeError("WORKERFS input data must be a Blob or File.");
          if (!file.name.startsWith("/") || file.name.lastIndexOf("/") <= 0) {
            throw new Error("WORKERFS input names must include a mount directory, for example /workerfs/input.mp4.");
          }
          // Blob/File is structured-cloned to the Worker. WORKERFS then reads only
          // the requested slices instead of copying the whole media file into MEMFS.
          files.push({ name: file.name, data: file.data, workerfs: true });
          continue;
        }
        const data = await toArrayBuffer(file.data);
        files.push({ name: file.name, data, workerfs: false });
        transfer.push(data);
      }
      const wasmForRun = this.wasmBytes.slice(0);
      transfer.push(wasmForRun);
      return await new Promise((resolve, reject) => {
        const created = createWorker(this.coreJsText);
        const worker = created.worker;
        const finish = () => { worker.terminate(); URL.revokeObjectURL(created.url); };
        worker.onmessage = (event) => {
          const message = event.data;
          if (message?.type === "log") { onLog({ stream: message.stream, message: message.message }); return; }
          if (message?.type === "progress") { onProgress(message.progress); return; }
          if (message?.type === "done") {
            finish();
            resolve({ exitCode: message.exitCode, files: message.files.map((file) => ({ name: file.name, data: new Uint8Array(file.data) })) });
            return;
          }
          if (message?.type === "error") {
            finish();
            const error = new Error(message.message); error.name = message.name || "Error"; error.stack = message.stack || error.stack; reject(error);
          }
        };
        worker.onerror = (event) => { finish(); reject(event.error || new Error(event.message || "FFmpeg WASM worker failed.")); };
        worker.postMessage({ wasmBytes: wasmForRun, args, files, outputs }, transfer);
      });
    }
    dispose() {
      if (this.disposed) return;
      this.disposed = true;
      for (const url of this.revokeUrls) URL.revokeObjectURL(url);
      this.revokeUrls = [];
      this.coreJsText = "";
      this.wasmBytes = new ArrayBuffer(0);
    }
  }

  async function loadHosted({ coreJsUrl, wasmUrl }) {
    assertSupported();
    const coreHref = new URL(coreJsUrl, document.baseURI).href;
    const wasmHref = new URL(wasmUrl, document.baseURI).href;
    const [coreResponse, wasmResponse] = await Promise.all([fetch(coreHref), fetch(wasmHref)]);
    if (!coreResponse.ok) throw new Error(`Failed to load FFmpeg JS: ${coreResponse.status} ${coreResponse.statusText}`);
    if (!wasmResponse.ok) throw new Error(`Failed to load FFmpeg WASM: ${wasmResponse.status} ${wasmResponse.statusText}`);
    return new FFmpegRunner(await coreResponse.text(), await wasmResponse.arrayBuffer());
  }

  async function loadEmbedded({ coreJsText, wasmBytes }) {
    assertSupported();
    return new FFmpegRunner(String(coreJsText), await toArrayBuffer(wasmBytes));
  }

  const videoCompressorArgs = (options = {}) => {
    const args = ["--input", options.input || "/input.bin", "--output", options.output || "/output.mp4"];
    const add = (name, value) => { if (value !== undefined && value !== null && value !== "") args.push(name, String(value)); };
    add("--max-width", options.maxWidth ?? 0);
    add("--max-height", options.maxHeight ?? 0);
    add("--fps", options.fps ?? 0);
    add("--crf", options.crf ?? 28);
    if (options.videoBitrateKbps) add("--video-bitrate", options.videoBitrateKbps);
    add("--preset", options.preset || "veryfast");
    add("--audio-bitrate", options.audioBitrateKbps ?? 128);
    if (options.noAudio) args.push("--no-audio");
    if (options.allowUpscale) args.push("--allow-upscale");
    if (options.faststart === false) args.push("--no-faststart");
    return args;
  };

  const losslessVideoCutterArgs = (options = {}) => {
    const args = ["--input", options.input || "/workerfs/input.mp4", "--output", options.output || "/output.mp4"];
    const add = (name, value) => { if (value !== undefined && value !== null && value !== "") args.push(name, String(value)); };
    add("--start", options.start ?? 0);
    if (options.end !== undefined && options.end !== null && options.end !== "") add("--end", options.end);
    if (options.noAudio) args.push("--no-audio");
    return args;
  };

  const mediaInspectorArgs = (options = {}) => [
    "--input", options.input || "/workerfs/input.bin",
    "--output", options.output || "/report.json"
  ];

  const videoContactSheetArgs = (options = {}) => {
    const count = Number(options.count ?? 12);
    const thumbSize = Number(options.thumbSize ?? 320);
    if (![12, 24, 48].includes(count)) throw new RangeError("Video Contact Sheet count must be 12, 24, or 48.");
    if (!Number.isInteger(thumbSize) || thumbSize < 96 || thumbSize > 640) throw new RangeError("Video Contact Sheet thumbSize must be an integer from 96 to 640.");
    const args = [
      "--input", options.input || "/workerfs/input.mp4",
      "--output", options.output || "/contact-sheet.ppm",
      "--count", String(count),
      "--thumb-size", String(thumbSize)
    ];
    if (options.columns !== undefined && options.columns !== null && options.columns !== "") {
      const columns = Number(options.columns);
      if (!Number.isInteger(columns) || columns < 1 || columns > Math.min(16, count)) throw new RangeError("Video Contact Sheet columns are out of range.");
      args.push("--columns", String(columns));
    }
    if (options.metadataOutput) {
      args.push("--metadata-output", String(options.metadataOutput));
    }
    return args;
  };

  const animationBaseArgs = (options = {}, defaultOutput) => {
    const number = (value, label, min, max, integer = false) => {
      const parsed = Number(value);
      if (!Number.isFinite(parsed) || parsed < min || parsed > max || (integer && !Number.isInteger(parsed))) {
        throw new RangeError(`${label} must be ${integer ? "an integer " : ""}from ${min} to ${max}.`);
      }
      return parsed;
    };
    const args = ["--input", options.input || "/workerfs/input.mp4", "--output", options.output || defaultOutput];
    const start = number(options.start ?? 0, "start", 0, 86400);
    args.push("--start", String(start));
    if (options.end !== undefined && options.end !== null && options.end !== "") {
      const end = number(options.end, "end", 0, 86400);
      if (end <= start) throw new RangeError("end must be greater than start.");
      args.push("--end", String(end));
    }
    args.push("--max-width", String(number(options.maxWidth ?? 480, "maxWidth", 0, 8192, true)));
    args.push("--max-height", String(number(options.maxHeight ?? 0, "maxHeight", 0, 8192, true)));
    args.push("--fps", String(number(options.fps ?? 15, "fps", 1, 60, true)));
    return args;
  };

  const videoToGifArgs = (options = {}) => {
    const args = animationBaseArgs(options, "/output.gif");
    const colors = Number(options.colors ?? 128);
    if (!Number.isInteger(colors) || colors < 16 || colors > 256) throw new RangeError("GIF colors must be an integer from 16 to 256.");
    const dither = String(options.dither || "sierra2_4a");
    if (!["sierra2_4a", "floyd_steinberg", "sierra2", "bayer", "heckbert"].includes(dither)) {
      throw new RangeError("Unsupported GIF dither mode.");
    }
    args.push("--colors", String(colors), "--dither", dither);
    return args;
  };

  const videoToWebpArgs = (options = {}) => {
    const args = animationBaseArgs(options, "/output.webp");
    const quality = Number(options.quality ?? 75);
    const compressionLevel = Number(options.compressionLevel ?? 4);
    if (!Number.isInteger(quality) || quality < 0 || quality > 100) throw new RangeError("WebP quality must be an integer from 0 to 100.");
    if (!Number.isInteger(compressionLevel) || compressionLevel < 0 || compressionLevel > 6) throw new RangeError("WebP compressionLevel must be an integer from 0 to 6.");
    args.push("--quality", String(quality), "--compression-level", String(compressionLevel), "--lossless", options.lossless ? "1" : "0");
    return args;
  };

  const decodePpmOutput = (result, name = "/contact-sheet.ppm") => {
    const file = result?.files?.find((item) => item.name === name);
    if (!file) throw new Error("PPM output was not returned: " + name);
    const bytes = file.data;
    let offset = 0;
    const isSpace = (value) => value === 9 || value === 10 || value === 13 || value === 32;
    const skip = () => {
      while (offset < bytes.length) {
        while (offset < bytes.length && isSpace(bytes[offset])) offset++;
        if (bytes[offset] !== 35) break;
        while (offset < bytes.length && bytes[offset] !== 10 && bytes[offset] !== 13) offset++;
      }
    };
    const token = () => {
      skip();
      const start = offset;
      while (offset < bytes.length && !isSpace(bytes[offset]) && bytes[offset] !== 35) offset++;
      if (start === offset) throw new Error("Invalid PPM header.");
      return new TextDecoder("ascii").decode(bytes.subarray(start, offset));
    };

    const magic = token();
    const width = Number(token());
    const height = Number(token());
    const maxValue = Number(token());
    if (magic !== "P6" || !Number.isInteger(width) || width <= 0 || !Number.isInteger(height) || height <= 0 || maxValue !== 255) {
      throw new Error("Unsupported PPM output header.");
    }
    if (offset >= bytes.length || !isSpace(bytes[offset])) throw new Error("Invalid PPM pixel-data boundary.");
    offset++;
    if (bytes[offset - 1] === 13 && bytes[offset] === 10) offset++;
    const expected = width * height * 3;
    if (!Number.isSafeInteger(expected) || bytes.length - offset !== expected) {
      throw new Error(`PPM pixel data size mismatch: expected ${expected}, got ${bytes.length - offset}.`);
    }
    return { width, height, pixels: bytes.subarray(offset) };
  };

  const decodeJsonOutput = (result, name = "/report.json") => {
    const file = result?.files?.find((item) => item.name === name);
    if (!file) throw new Error("JSON output was not returned: " + name);
    return JSON.parse(new TextDecoder().decode(file.data));
  };

  window.BrowserFFmpeg = Object.freeze({
    loadHosted,
    loadEmbedded,
    videoCompressorArgs,
    losslessVideoCutterArgs,
    mediaInspectorArgs,
    videoContactSheetArgs,
    videoToGifArgs,
    videoToWebpArgs,
    decodePpmOutput,
    decodeJsonOutput,
    isSupported
  });
})();
