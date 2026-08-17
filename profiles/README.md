# Build profiles

Each app has one FFmpeg configure flag file and one public-libav runner:

```text
profiles/<profile>/ffmpeg.flags
runners/<profile>.c
```

A profile may also provide `profiles/<profile>/single-html/template.html` for one-file packaging.

`ffmpeg.flags` controls which FFmpeg codecs/demuxers/muxers/parsers/protocols/filters are compiled. The C runner determines which operations the browser API actually supports. Both need to be updated when adding a new feature.
