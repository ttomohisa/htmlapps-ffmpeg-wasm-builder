param()

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest
$Root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)

function Require-File([string]$RelativePath) {
  $path = Join-Path $Root $RelativePath
  if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Required file is missing: $RelativePath" }
  $item = Get-Item -LiteralPath $path
  if ($item.Length -eq 0) { throw "Required file is empty: $RelativePath" }
  return $item.FullName
}
function Optional-File([string]$RelativePath) {
  $path = Join-Path $Root $RelativePath
  # Optional files must never make repository validation fail.
  # Use File.Exists directly instead of Test-Path + Get-Item because some
  # CI/filesystem combinations can report a transient/virtual path to
  # Test-Path that Get-Item cannot subsequently resolve.
  if (-not [IO.File]::Exists($path)) { return $null }
  return $path
}
function Require-Text([string]$Path, [string]$Needle, [string]$Message) {
  $text = [IO.File]::ReadAllText($Path)
  if (-not $text.Contains($Needle)) { throw $Message }
}
function Forbid-Path([string]$RelativePath) {
  $path = Join-Path $Root $RelativePath
  if (Test-Path $path) { throw "Removed CLI/legacy path must not return: $RelativePath" }
}

$versions = Require-File "versions.env"
$gitattributes = Optional-File ".gitattributes"
if ($null -eq $gitattributes) {
  Write-Warning ".gitattributes is missing. The build can continue, but keeping it is recommended so shell scripts stay LF on Windows clones."
}
$dockerfile = Require-File "docker/Dockerfile"
$buildScript = Require-File "scripts/build-ffmpeg.sh"
$windowsBuild = Require-File "scripts/build.ps1"
$unixBuild = Require-File "build.sh"
$runtime = Require-File "runtime/browser-ffmpeg.js"
$runner = Require-File "runners/video-compressor.c"
$profile = Require-File "profiles/video-compressor/ffmpeg.flags"
$template = Require-File "profiles/video-compressor/single-html/template.html"
$packer = Require-File "scripts/pack-single-html.ps1"
$smokePacker = Require-File "scripts/pack-smoke-test.sh"
$smokeTemplate = Require-File "tests/smoke-test.template.html"
$smokeFixture = Require-File "tests/fixtures/smoke-input.mp4"
$smokeWindows = Require-File "scripts/smoke-test.ps1"
$smokeUnix = Require-File "scripts/smoke-test.sh"
$releaseScript = Require-File "scripts/prepare-release.sh"
$releaseWorkflow = Require-File ".github/workflows/release.yml"
$thirdParty = Require-File "THIRD_PARTY_NOTICES.md"
$licenseIndex = Require-File "LICENSES/README.md"
$licenseDoc = Require-File "docs/LICENSES.md"
$releaseDoc = Require-File "docs/RELEASING.md"
$readme = Require-File "README.md"
$readmeEn = Require-File "README.en.md"

foreach ($legacy in @(
  "build-cli.bat",
  "build-compact.bat",
  "scripts/build-cli.sh",
  "scripts/build-compact.sh",
  "runtime/browser-ffmpeg-cli.js",
  "runtime/browser-ffmpeg-compact.js",
  "profiles/video-compressor/cli.flags",
  "profiles/video-compressor/compact.flags",
  "profiles/video-compressor/single-html/cli.template.html",
  "profiles/video-compressor/single-html/compact.template.html",
  "compact"
)) { Forbid-Path $legacy }

Require-Text $buildScript "--disable-pthreads" "WASM build must disable pthreads."
Require-Text $buildScript "--disable-programs" "WASM build must not link the upstream ffmpeg CLI."
Require-Text $buildScript "-sEXPORT_NAME=createFFmpegCore" "WASM factory name must stay stable."
Require-Text $buildScript "INCOMING_MODULE_JS_API=wasmBinary,instantiateWasm,locateFile,print,printErr" "WASM build must preserve custom loader hooks."
Require-Text $buildScript '"emscriptenCommit": "$EMSCRIPTEN_COMMIT"' "Manifest must record the exact Emscripten source commit."
$buildText = [IO.File]::ReadAllText($buildScript)
if ($buildText -match '(^|\s)-pthread(\s|$)') { throw "WASM build must not link with -pthread." }

Require-Text $runtime "instantiateWasm" "Browser runtime must instantiate transferred Wasm bytes directly."
Require-Text $runtime "new Blob([coreJsText" "Browser runtime must combine generated core JS and Worker body into one Blob."
Require-Text $runtime "window.BrowserFFmpeg" "Browser runtime must expose BrowserFFmpeg."
$runtimeText = [IO.File]::ReadAllText($runtime)
if ($runtimeText.Contains("importScripts(")) { throw "Browser runtime must not use nested importScripts; file:// blob origins are not portable." }
if ($runtimeText -match 'typeof\s+SharedArrayBuffer|crossOriginIsolated') { throw "Browser runtime must not depend on SharedArrayBuffer/cross-origin isolation." }

$runnerText = [IO.File]::ReadAllText($runner)
if ($runnerText -match 'pthread_(create|join|mutex|cond)') { throw "Runner must not call pthread APIs." }
Require-Text $runner '#define RUNNER_VERSION "1.0.0"' "Runner version must be 1.0.0."
Require-Text $runner "avformat_open_input" "Runner must use public libavformat APIs."
Require-Text $runner "avcodec_send_packet" "Runner decode loop is missing."
Require-Text $runner 'av_opt_set_array(sink_ctx, "pixel_formats"' "Runner must use FFmpeg 9+ pixel_formats array option."
Require-Text $runner 'av_opt_set_array(sink_ctx, "sample_formats"' "Runner must use FFmpeg 9+ sample_formats array option."
Require-Text $runner 'av_opt_set_array(sink_ctx, "samplerates"' "Runner must use FFmpeg 9+ samplerates array option."
Require-Text $runner 'av_opt_set_array(sink_ctx, "channel_layouts"' "Runner must use FFmpeg 9+ channel_layouts array option."
foreach ($deprecated in @('"pix_fmts"', '"sample_fmts"', '"sample_rates"', '"ch_layouts"')) {
  if ($runnerText.Contains($deprecated)) { throw "Runner must not use removed FFmpeg 9 buffer-sink option $deprecated." }
}

Require-Text $packer "__FFMPEG_JS_GZIP_BASE64__" "Single-HTML packer is missing the JS payload token."
Require-Text $packer "__FFMPEG_WASM_GZIP_BASE64__" "Single-HTML packer is missing the Wasm payload token."
Require-Text $template "BrowserFFmpeg.loadEmbedded" "Single-HTML template must use the embedded runtime."

Require-Text $smokeTemplate "BrowserFFmpeg.videoCompressorArgs" "Smoke test must run the actual video-compressor runner."
Require-Text $smokeTemplate "SMOKE_TEST_PASS" "Smoke test PASS sentinel is missing."
Require-Text $smokeTemplate 'containsAscii(output, "moov")' "Smoke test must validate the MP4 moov box."
Require-Text $smokeTemplate 'containsAscii(output, "mdat")' "Smoke test must validate the MP4 mdat box."
Require-Text $smokeTemplate 'containsAscii(output, "avc1")' "Smoke test must validate H.264/avc1 output."
Require-Text $smokeTemplate 'containsAscii(output, "mp4a")' "Smoke test must validate AAC/mp4a output."
Require-Text $smokePacker "smoke-input.mp4" "Smoke-test packer must embed the fixture."
Require-Text $windowsBuild "smoke-test.ps1" "Windows build must run the browser smoke test automatically."
Require-Text $unixBuild "smoke-test.sh" "Unix/CI build must run the browser smoke test automatically."
Require-Text $smokeWindows "--remote-debugging-port=0" "Smoke runner must launch a real headless browser with DevTools enabled."
Require-Text $smokeWindows "SMOKE_TEST_PASS_bytes" "Smoke runner must wait for the browser PASS sentinel."
Require-Text $smokeUnix "smoke-test.ps1" "Unix/CI wrapper must use the same browser smoke-test implementation."

$fixtureBytes = [IO.File]::ReadAllBytes($smokeFixture)
if ($fixtureBytes.Length -lt 1024) { throw "Smoke input MP4 is unexpectedly small." }
if ([Text.Encoding]::ASCII.GetString($fixtureBytes, 4, 4) -ne "ftyp") { throw "Smoke input fixture is not an MP4 file." }

$windowsBuildText = [IO.File]::ReadAllText($windowsBuild)
$unixBuildText = [IO.File]::ReadAllText($unixBuild)
if ($windowsBuildText.Contains('"--builder", "default"')) { throw "Windows build must not force a Buildx builder across Docker contexts." }
if ($unixBuildText -match '--builder[ =]+default') { throw "Unix build must not force a Buildx builder across Docker contexts." }
Require-Text $windowsBuild '"buildx", "build"' "Windows build must use Buildx."
Require-Text $unixBuild "docker buildx build" "Unix build must use Buildx."
Require-Text $windowsBuild 'EMSCRIPTEN_COMMIT' "Windows build must pass the Emscripten source commit into Docker."
Require-Text $unixBuild 'EMSCRIPTEN_COMMIT' "Unix build must pass the Emscripten source commit into Docker."
if ($null -ne $gitattributes) {
  Require-Text $gitattributes "*.sh text eol=lf" "Shell scripts must stay LF across Windows clones."
  Require-Text $gitattributes "*.bat text eol=crlf" "Windows batch launchers must use CRLF."
  Require-Text $gitattributes "*.mp4 binary" "Smoke fixture must be marked binary."
}

$versionsText = [IO.File]::ReadAllText($versions)
if ($versionsText -notmatch '(?m)^BUILDER_VERSION=1\.0\.0$') { throw "Builder version must be 1.0.0." }
foreach ($requiredPin in @(
  'EMSDK_VERSION', 'EMSCRIPTEN_REPOSITORY', 'EMSCRIPTEN_REF', 'EMSCRIPTEN_COMMIT',
  'FFMPEG_REPOSITORY', 'FFMPEG_REF', 'FFMPEG_COMMIT',
  'X264_REPOSITORY', 'X264_FALLBACK_REPOSITORY', 'X264_REF', 'X264_COMMIT'
)) {
  if ($versionsText -notmatch "(?m)^$requiredPin=.+$") { throw "versions.env is missing $requiredPin." }
}
foreach ($commitName in @('EMSCRIPTEN_COMMIT', 'FFMPEG_COMMIT', 'X264_COMMIT')) {
  $match = [regex]::Match($versionsText, "(?m)^$commitName=([0-9a-f]{40})$")
  if (-not $match.Success) { throw "$commitName must be a full 40-character lowercase hex commit." }
}

$dockerText = [IO.File]::ReadAllText($dockerfile)
if ($dockerText -match 'cli-builder|export-cli|export-compact|export-all|build-cli') { throw "Dockerfile still contains removed dual-mode stages." }
Require-Text $dockerfile "FROM scratch AS export" "Dockerfile must expose one clean export target."
Require-Text $dockerfile "EMSCRIPTEN_COMMIT" "Dockerfile must carry the exact Emscripten source commit into the toolchain environment."

Require-Text $thirdParty "generated `ffmpeg.wasm`" "Third-party notice must distinguish generated Wasm from the MIT builder source."
Require-Text $thirdParty "GPL-2.0-or-later" "Third-party notice must state the generated core license."
Require-Text $licenseIndex "FFmpeg-COPYING.GPLv2" "License index must document FFmpeg GPL license packaging."
Require-Text $licenseDoc "same GitHub Release" "Licensing docs must keep binaries and corresponding source together."
Require-Text $readme "GPL-2.0-or-later" "Japanese README must clearly state the generated core license."
Require-Text $readme "THIRD_PARTY_NOTICES.md" "Japanese README must link third-party notices."
Require-Text $readmeEn "does **not** relicense generated `ffmpeg.wasm`" "English README must clearly scope the root MIT license."
Require-Text $releaseDoc "git tag -a v1.0.0" "Release documentation must include the first stable tag procedure."

Require-Text $releaseScript 'fetch_exact "FFmpeg"' "Release packer must fetch exact FFmpeg source."
Require-Text $releaseScript 'fetch_exact "x264"' "Release packer must fetch exact x264 source."
Require-Text $releaseScript 'fetch_exact "Emscripten"' "Release packer must fetch exact Emscripten source."
Require-Text $releaseScript "FFmpeg-COPYING.GPLv2" "Release binary bundle must include FFmpeg GPL license text."
Require-Text $releaseScript "x264-COPYING" "Release binary bundle must include x264 license text."
Require-Text $releaseScript "Emscripten-LICENSE" "Release binary bundle must include Emscripten license text."
Require-Text $releaseScript "Emscripten-musl-COPYRIGHT" "Release binary bundle must include musl copyright/license notice."
Require-Text $releaseScript "Emscripten-compiler-rt-LICENSE.txt" "Release binary bundle must include compiler-rt license text."
Require-Text $releaseScript "ffmpeg-wasm-sources-v" "Release packer must create a corresponding-source archive."
Require-Text $releaseScript "sha256sum" "Release packer must generate SHA-256 checksums."
Require-Text $releaseScript "BUILDER_COPY" "Release source bundle must include the Builder recipe."

Require-Text $releaseWorkflow 'tags:' "Release workflow must be tag-driven."
Require-Text $releaseWorkflow 'test "${GITHUB_REF_NAME}" = "v${BUILDER_VERSION}"' "Release workflow must verify tag/version equality."
Require-Text $releaseWorkflow "./build.sh video-compressor" "Release workflow must rebuild and run the smoke test before publishing."
Require-Text $releaseWorkflow "prepare-release.sh" "Release workflow must prepare binary/source bundles."
Require-Text $releaseWorkflow "gh release create" "Release workflow must create a GitHub Release."
Require-Text $releaseWorkflow "--verify-tag" "Release creation must refuse an unpushed/missing tag."
Require-Text $releaseWorkflow "contents: write" "Release workflow needs explicit contents:write permission."

$node = Get-Command node -ErrorAction SilentlyContinue
if ($node) {
  & node --check $runtime
  if ($LASTEXITCODE -ne 0) { throw "JavaScript syntax check failed: runtime/browser-ffmpeg.js" }
}

Write-Host "[OK] Repository checks passed." -ForegroundColor Green
