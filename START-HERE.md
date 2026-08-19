# START HERE — 知識ゼロからの手順

Windows側へC compilerやEmscriptenを手作業で入れる必要はありません。Docker DesktopのLinux engine内でビルドします。

## 1. Docker Desktopを起動

初回はEmscripten imageやFFmpeg sourceを取得するため時間がかかります。2回目以降はDocker cacheが効きます。

## 2. 作りたいWASMを選ぶ

動画圧縮：

```text
build.bat video-compressor
```

無劣化動画カット：

```text
build-lossless-video-cutter.bat
```

Media Inspector：

```text
build-media-inspector.bat
```

Video Contact Sheetを作る場合：

```text
build-video-contact-sheet.bat
build-video-to-gif.bat
build-video-to-webp.bat
```

6 profileとも最後にheadless Chrome / Edgeで実処理のsmoke testが自動実行されます。

```text
[OK] Smoke test passed.
```

まで出れば成功です。

## 3. 生成物

```text
dist\<profile>\
  ffmpeg.js
  ffmpeg.wasm
  ffmpeg.js.gz
  ffmpeg.wasm.gz
  manifest.json
  smoke-test.html
```

各profileは `dist\<profile>\` に出力されます。Lossless Cutter / Media Inspector / Video Contact Sheet / Video to GIF / Video to WebP は大きな入力File/BlobをWORKERFS経由で読み込めます。Animation profileは短い区間だけdecodeし、GIFまたはAnimated WebPをMEMFSへ出力します。

## 4. FFmpeg更新

```text
check-updates.bat
```

更新後はmainのGitHub Actionsが全release profileをbuild + browser smoke testします。

## 5. 新しいツールを増やす

`profiles/<profile>/profile.env`、`ffmpeg.flags`、`runners/<profile>.c`、`tests/smoke-tests/<profile>.js` を1セットとして追加します。詳しくは `docs/ADDING_PROFILE.md` を参照してください。
