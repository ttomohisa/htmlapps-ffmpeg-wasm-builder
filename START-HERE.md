# START HERE — 知識ゼロからの手順

このリポジトリは「FFmpegをブラウザー用WASMへ自分でビルドし、実際に動画変換できるところまで自動確認する」ためのものです。Windows側へCコンパイラやEmscriptenを手作業で入れる必要はありません。Dockerの中でビルドします。

## 1. Docker Desktopを起動する

Docker Desktopをインストールし、Linux engineの起動完了まで待ちます。

初回はEmscriptenのDocker imageを取得するため時間がかかります。2回目以降はDocker cacheが効きます。

## 2. ビルドする

```text
build.bat
```

成功すると：

```text
dist\video-compressor\
  ffmpeg.js
  ffmpeg.wasm
  ffmpeg.js.gz
  ffmpeg.wasm.gz
  manifest.json
  smoke-test.html
```

ができます。

## 3. smoke testは自動

ビルド後にMicrosoft EdgeまたはChromeをheadlessで起動し、小さなテスト動画を実際に変換します。

```text
入力 H.264 + AAC MP4
      ↓
FFmpeg WASM runner
      ↓
出力 H.264 + AAC MP4
      ↓
ftyp / moov / mdat / avc1 / mp4a を検証
```

最後に

```text
[OK] Smoke test passed.
```

が出れば、単なるコンパイル成功ではなくブラウザーでの実変換まで通っています。

ブラウザーを自動検出できない場合は、Microsoft EdgeかGoogle Chromeをインストールしてください。必要なら環境変数 `FFMPEG_WASM_BROWSER` に実行ファイルのパスを指定できます。

## 4. 単一HTMLを作る

```text
pack-single-html.bat
```

生成：

```text
dist\single-html-video-compressor.html
```

ダブルクリックして動画変換できます。SharedArrayBuffer / COOP / COEPは不要です。

すぐ画面を開きたい場合は：

```text
demo.bat
```

## 5. FFmpegを更新するとき

```text
check-updates.bat
```

候補を確認し、`versions.env` の1項目だけ変更して `build.bat` を実行します。smoke testまで通れば、その更新は少なくともvideo-compressorの基本経路では動作しています。

## 6. 機能を追加するとき

- FFmpeg component → `profiles/video-compressor/ffmpeg.flags`
- 実際の変換処理 → `runners/video-compressor.c`
- ブラウザーAPI → `runtime/browser-ffmpeg.js`

次のFFmpegアプリを追加する手順は `docs/ADDING_PROFILE.md` にあります。


## 7. publicにしてReleaseする場合

最初は `main` だけpushし、GitHub Actionsのビルド＋smoke testが成功することを確認してください。その後で `versions.env` の `BUILDER_VERSION` と一致するタグをpushします。v1.0.0なら：

```text
git tag -a v1.0.0 -m "FFmpeg WASM Builder v1.0.0"
git push origin v1.0.0
```

タグReleaseの詳細は `docs/RELEASING.md` を参照してください。
