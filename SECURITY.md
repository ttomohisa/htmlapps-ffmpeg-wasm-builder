# Security

- FFmpeg network support is disabled by default.
- User media is intended to stay inside the browser.
- Hosted use fetches only the application's own JS/Wasm assets.
- The runtime does not require SharedArrayBuffer or cross-origin isolation.
- FFmpeg/Emscripten/x264 versions are pinned for reproducible builds.
- Review upstream security releases before upgrading pins.
- Do not enable arbitrary FFmpeg network protocols unless a specific app requires them.
