# Video Audio Extractor profile

Browser Kitty の Video Audio Extractor 向け public-libav profile です。

Phase 1 では入力を WORKERFS で読み、次の2操作だけを提供します。

- container / stream header から動画数と音声track一覧を JSON で返す
- 検証済みの codec/container 組み合わせについて、選択した音声streamを再圧縮せずに1本だけ保存する

動画・音声 decoder / encoder、libavfilter、swscale、swresample、x264 はリンクしません。入力 File/Blob 全体を MEMFS へ複製せず、大容量動画でも必要な範囲を Worker から読み取れる構成です。出力は従来どおり MEMFS に生成します。

## Runner API

Inspection:

```text
--input /workerfs/input.bin
--inspect-output /report.json
```

Stream copy:

```text
--input /workerfs/input.bin
--audio-stream 1
--copy-format m4a
--output /output.m4a
```

`--audio-stream` は FFmpeg の実stream indexです。アプリ一般画面ではこれを直接見せず、inspection JSON から選択したtrackの index をrunnerへ渡します。

Phase 1 の許可済みcopy targetは次のとおりです。

| Input audio codec | `copy-format` | Output |
|---|---|---|
| AAC | `m4a` | M4A |
| ALAC | `m4a` | M4A |
| MP3 | `mp3` | MP3 |
| Opus | `opus` | Ogg Opus (`.opus`) |
| Vorbis | `ogg` | Ogg Vorbis |
| FLAC | `flac` | FLAC |
| supported PCM | `wav` | WAV |
| AC-3 | `ac3` | AC3 |
| E-AC-3 | `eac3` | EAC3 |

MPEG-TS / raw AAC を M4A へ移す場合は `aac_adtstoasc` bitstream filterを使います。それ以外の codec を推測でremuxせず、inspection JSONでは `copy.supported=false` として返します。

MP3 / M4A / WAVへの再エンコードは後続Phaseで追加します。
