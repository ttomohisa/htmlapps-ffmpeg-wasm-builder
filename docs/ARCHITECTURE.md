# Architecture

FFmpeg WASM Builder pins FFmpeg/Emscripten and optional codec libraries, applies a profile, links a small public-libav runner, and validates the generated browser runtime with a real smoke test. The upstream `ffmpeg` CLI is not linked.

## Threading model

Single-thread remains the default. Existing release profiles do not opt into pthreads and preserve their SharedArrayBuffer-free, `file://`-friendly behavior.

A profile may explicitly declare `PROFILE_THREADING_VARIANTS`. `ffmpeg-filter-builder` is the first profile with `single-thread,multi-thread`. `build.ps1` / `build.sh` execute both variants from the same profile/runner sources. FFmpeg and x264 receive the same `THREADING_MODE`, preventing an MT application wrapper from accidentally using an ST codec core.

The initial Filter Builder MT runtime uses an 8-worker prewarmed pthread pool with explicit budgets: 2 video decoder workers, 4 libx264 encoder workers, and 1 x264 lookahead worker. Audio codecs and filtergraph threading are initially bounded to one thread. This keeps the strict pool below exhaustion while prioritizing the more expensive H.264 encode path. With Emscripten 6.x, the MT output does not require a separate pthread worker file: `ffmpeg.js` itself is reused as the pthread Worker program. Browser integration provides the embedded main JS as a Blob-backed `mainScriptUrlOrBlob`; MT startup requires `SharedArrayBuffer` and `crossOriginIsolated`. ST does not.

## Output compatibility

Legacy profiles remain at `dist/<profile>/`. Dual profiles use `dist/<profile>/<variant>/`. This avoids breaking existing consumer scripts.

## Profiles and manifests

`profiles/<profile>/profile.env` defines optional libraries, linked FFmpeg archives, required config markers, catalog metadata, threading variants, and the pthread pool size. Manifest schema 8 records threading, pool size, SharedArrayBuffer/cross-origin-isolation requirements, catalog entries, dependencies, and generated worker hashes when present.

## Filter Builder timeline ranges

The v1.9.5 Filter Builder runner keeps range rendering inside the public-libav path. `--start-time` / `--duration` prepend `trim,setpts` to video and `atrim,asetpts` to audio. When a finite duration is requested, the demux loop stops after the requested end plus a one-second decode guard so preview renders do not continue through the full source. Exact bounds remain enforced by the filters.
