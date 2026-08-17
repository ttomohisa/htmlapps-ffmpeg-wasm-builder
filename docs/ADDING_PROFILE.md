# Adding another FFmpeg-powered app

A profile owns the FFmpeg components, a small public-libav runner, and an optional single-HTML shell.

For `audio-converter`:

```text
profiles/audio-converter/
├─ ffmpeg.flags
└─ single-html/
   └─ template.html

runners/audio-converter.c
```

Start by copying the video-compressor profile and runner. Then remove codecs, demuxers, muxers, parsers and filters that the new app does not need. Because profiles use `--disable-everything`, every required component must be explicitly enabled or selected as a dependency.

The runner should stay on public FFmpeg headers/APIs. Do not copy `fftools` internals. Keep pthreads disabled unless the architecture is intentionally redesigned.

Build the profile with:

```text
build.bat audio-converter
```

For every new profile, also add a small representative media fixture and adjust the smoke-test path so the CI verifies a real successful operation for that profile. A compile-only test is not sufficient for version-upgrade confidence.
