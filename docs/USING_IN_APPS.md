# Using the generated Wasm in apps

## Recommended distribution model

Use a **fixed FFmpeg WASM Builder GitHub Release at app update/build time**. Do not have normal production browser sessions download their core from GitHub Releases.

```text
Builder v1.0.0 Release
        ↓ download while updating the app
app repository/build step
        ↓ pin/copy/embed
app's own hosted assets or single HTML
        ↓
end-user browser (no GitHub runtime dependency)
```

The release asset `ffmpeg-wasm-video-compressor-v1.0.0.zip` contains:

```text
ffmpeg.js
ffmpeg.wasm
ffmpeg.js.gz
ffmpeg.wasm.gz
manifest.json
browser-ffmpeg.js
BUILDINFO.txt
THIRD_PARTY_NOTICES.md
LICENSES/
```

The same GitHub Release contains `ffmpeg-wasm-sources-v1.0.0.tar.gz` with exact corresponding source. When redistributing the generated core, keep the license/source notice reachable from the consuming project.

## Hosted assets

If the consuming app hosts separate assets, copy the generated files to a versioned same-origin path, for example:

```text
/assets/ffmpeg/builder-v1.0.0/video-compressor/
├─ ffmpeg.js
└─ ffmpeg.wasm
```

Initialize lazily:

```js
const runner = await BrowserFFmpeg.loadHosted({
  coreJsUrl: "/assets/ffmpeg/builder-v1.0.0/video-compressor/ffmpeg.js",
  wasmUrl: "/assets/ffmpeg/builder-v1.0.0/video-compressor/ffmpeg.wasm"
});

const result = await runner.run({
  files: [{ name: "/input.bin", data: file }],
  outputs: ["/output.mp4"],
  args: BrowserFFmpeg.videoCompressorArgs({
    maxWidth: 1280,
    crf: 28,
    audioBitrateKbps: 128
  })
});
```

Show the UI first and load the FFmpeg assets only when the user selects a video or starts processing. Version the URLs and use long immutable caching.

## Single HTML

Run `pack-single-html.bat` in this Builder, or use the same embedding strategy in the consuming app. Generated gzip JS/Wasm can be embedded and expanded only when conversion begins. The runtime is designed to work from `file://` without SharedArrayBuffer or COOP/COEP headers.

## API scope

`BrowserFFmpeg.videoCompressorArgs()` is deliberately small and does not accept arbitrary FFmpeg CLI syntax. Add capabilities to the profile + public-libav runner instead of exposing unrestricted FFmpeg command parsing.

## License/source handoff

A consuming application should keep a clear notice that its embedded/generated FFmpeg/x264 Wasm is GPL-2.0-or-later and point users to the exact Builder release used. The Builder release carries `BUILDINFO.txt`, upstream license files, and corresponding source.
