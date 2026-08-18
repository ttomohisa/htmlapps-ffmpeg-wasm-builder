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
  if (-not [IO.File]::Exists($path)) { return $null }
  return $path
}
function Require-Text([string]$Path, [string]$Needle, [string]$Message) {
  $text = [IO.File]::ReadAllText($Path)
  if (-not $text.Contains($Needle)) {
    throw ("{0} Missing literal in {1}: {2}" -f $Message, $Path, $Needle)
  }
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
$dockerCommon = Require-File "scripts/docker-common.sh"
$windowsBuild = Require-File "scripts/build.ps1"
$unixBuild = Require-File "build.sh"
$runtime = Require-File "runtime/browser-ffmpeg.js"
$videoRunner = Require-File "runners/video-compressor.c"
$cutterRunner = Require-File "runners/lossless-video-cutter.c"
$videoProfile = Require-File "profiles/video-compressor/ffmpeg.flags"
$videoProfileEnv = Require-File "profiles/video-compressor/profile.env"
$cutterProfile = Require-File "profiles/lossless-video-cutter/ffmpeg.flags"
$cutterProfileEnv = Require-File "profiles/lossless-video-cutter/profile.env"
$cutterReadme = Require-File "profiles/lossless-video-cutter/README.md"
$cutterTemplate = Require-File "profiles/lossless-video-cutter/single-html/template.html"
$template = Require-File "profiles/video-compressor/single-html/template.html"
$packer = Require-File "scripts/pack-single-html.ps1"
$smokePacker = Require-File "scripts/pack-smoke-test.sh"
$smokeTemplate = Require-File "tests/smoke-test.template.html"
$videoSmoke = Require-File "tests/smoke-tests/video-compressor.js"
$cutterSmoke = Require-File "tests/smoke-tests/lossless-video-cutter.js"
$smokeFixture = Require-File "tests/fixtures/smoke-input.mp4"
$smokeWindows = Require-File "scripts/smoke-test.ps1"
$smokeUnix = Require-File "scripts/smoke-test.sh"
$releaseScript = Require-File "scripts/prepare-release.sh"
$releaseWorkflow = Require-File ".github/workflows/release.yml"
$buildWorkflow = Require-File ".github/workflows/build.yml"
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

Require-Text $dockerCommon "load_profile_config" "Profile metadata loader is missing."
Require-Text $buildScript '"${PROFILE_REQUIRED_CONFIG[@]}"' "Build must assert profile-specific FFmpeg config."
Require-Text $buildScript '"${PROFILE_LINK_LIBS[@]}"' "Build must link profile-specific FFmpeg libraries."
Require-Text $buildScript 'PROFILE_USE_X264' "Build must make x264 profile-specific."
Require-Text $buildScript 'PROFILE_USE_WORKERFS' "Build must make WORKERFS profile-specific."
Require-Text $buildScript '-lworkerfs.js' "WORKERFS profiles must explicitly link Emscripten WORKERFS."
Require-Text $buildScript 'WORKERFS' "WORKERFS must be exported to the browser runtime when enabled."
Require-Text $buildScript "--disable-pthreads" "WASM build must disable pthreads."
Require-Text $buildScript "--disable-programs" "WASM build must not link the upstream ffmpeg CLI."
Require-Text $buildScript "-sEXPORT_NAME=createFFmpegCore" "WASM factory name must stay stable."
Require-Text $buildScript "INCOMING_MODULE_JS_API=wasmBinary,instantiateWasm,locateFile,print,printErr" "WASM build must preserve custom loader hooks."
Require-Text $buildScript '"schemaVersion": 5' "Manifest schema must include profile-aware metadata."
Require-Text $buildScript '"x264Linked":' "Manifest must state whether x264 is linked."
$buildText = [IO.File]::ReadAllText($buildScript)
if ($buildText -match '(^|\s)-pthread(\s|$)') { throw "WASM build must not link with -pthread." }

Require-Text $runtime "instantiateWasm" "Browser runtime must instantiate transferred Wasm bytes directly."
Require-Text $runtime "new Blob([coreJsText" "Browser runtime must combine generated core JS and Worker body into one Blob."
Require-Text $runtime "losslessVideoCutterArgs" "Browser runtime must expose Lossless Video Cutter args."
Require-Text $runtime "mountWorkerFiles" "Browser runtime must support Blob/File-backed WORKERFS mounts."
Require-Text $runtime "file.workerfs === true" "Browser runtime must keep WORKERFS inputs out of the ArrayBuffer/MEMFS path."
Require-Text $runtime "window.BrowserFFmpeg" "Browser runtime must expose BrowserFFmpeg."
$runtimeText = [IO.File]::ReadAllText($runtime)
if ($runtimeText.Contains("importScripts(")) { throw "Browser runtime must not use nested importScripts; file:// blob origins are not portable." }
if ($runtimeText -match 'typeof\s+SharedArrayBuffer|crossOriginIsolated') { throw "Browser runtime must not depend on SharedArrayBuffer/cross-origin isolation." }

$videoRunnerText = [IO.File]::ReadAllText($videoRunner)
if ($videoRunnerText -match 'pthread_(create|join|mutex|cond)') { throw "Video runner must not call pthread APIs." }
Require-Text $videoRunner '#define RUNNER_VERSION "1.1.0"' "Video runner version must be 1.1.0."
Require-Text $videoRunner "avcodec_send_packet" "Video runner decode loop is missing."
Require-Text $videoRunner 'av_opt_set_array(sink_ctx, "pixel_formats"' "Video runner must use FFmpeg 9+ pixel_formats array option."
Require-Text $videoRunner 'av_opt_set_array(sink_ctx, "sample_formats"' "Video runner must use FFmpeg 9+ sample_formats array option."
Require-Text $videoRunner 'av_opt_set_array(sink_ctx, "samplerates"' "Video runner must use FFmpeg 9+ samplerates array option."
Require-Text $videoRunner 'av_opt_set_array(sink_ctx, "channel_layouts"' "Video runner must use FFmpeg 9+ channel_layouts array option."
foreach ($deprecated in @('"pix_fmts"', '"sample_fmts"', '"sample_rates"', '"ch_layouts"')) {
  if ($videoRunnerText.Contains($deprecated)) { throw "Video runner must not use removed FFmpeg 9 buffer-sink option $deprecated." }
}

$cutterRunnerText = [IO.File]::ReadAllText($cutterRunner)
if ($cutterRunnerText -match 'pthread_(create|join|mutex|cond)') { throw "Cutter runner must not call pthread APIs." }
Require-Text $cutterRunner '#define RUNNER_VERSION "1.1.0"' "Cutter runner version must be 1.1.0."
Require-Text $cutterRunner "av_seek_frame" "Cutter must seek to a keyframe."
Require-Text $cutterRunner "AV_PKT_FLAG_KEY" "Cutter must anchor output to a keyframe."
Require-Text $cutterRunner "avcodec_parameters_copy" "Cutter must stream-copy codec parameters."
Require-Text $cutterRunner "av_packet_rescale_ts" "Cutter must rescale packet timestamps."
Require-Text $cutterRunner "av_interleaved_write_frame" "Cutter must remux packets without decoding."
Require-Text $cutterRunner "actual-start=" "Cutter must report the keyframe-aligned actual start."
foreach ($forbiddenApi in @("avcodec_send_packet", "avcodec_receive_frame", "avcodec_send_frame", "avcodec_receive_packet", "avfilter_graph_alloc")) {
  if ($cutterRunnerText.Contains($forbiddenApi)) { throw "Lossless cutter must not decode/encode/filter: $forbiddenApi" }
}

Require-Text $videoProfileEnv "PROFILE_USE_X264=1" "Video profile must link x264."
Require-Text $videoProfileEnv "PROFILE_USE_WORKERFS=0" "Video profile should not carry WORKERFS overhead."
Require-Text $cutterProfileEnv "PROFILE_USE_X264=0" "Lossless cutter must not link x264."
Require-Text $cutterProfileEnv "PROFILE_USE_WORKERFS=1" "Lossless cutter must expose large File/Blob input through WORKERFS."
Require-Text $cutterProfileEnv 'PROFILE_BINARY_LICENSE="LGPL-2.1-or-later"' "Lossless cutter should remain LGPL without GPL-only components."
Require-Text $cutterProfileEnv "libavformat/libavformat.a" "Cutter must link libavformat."
Require-Text $cutterProfileEnv "libavcodec/libavcodec.a" "Cutter must link libavcodec packet/codec-parameter APIs."
Require-Text $cutterProfileEnv "libavutil/libavutil.a" "Cutter must link libavutil."
$cutterProfileEnvText = [IO.File]::ReadAllText($cutterProfileEnv)
foreach ($heavy in @("libavfilter/libavfilter.a", "libswscale/libswscale.a", "libswresample/libswresample.a")) {
  if ($cutterProfileEnvText.Contains($heavy)) { throw "Lossless cutter should not link heavy processing library: $heavy" }
}
$cutterFlagsText = [IO.File]::ReadAllText($cutterProfile)
foreach ($forbiddenFlag in @("--enable-decoder=", "--enable-encoder=", "--enable-filter=", "--enable-libx264", "--enable-gpl")) {
  if ($cutterFlagsText.Contains($forbiddenFlag)) { throw "Lossless cutter flags must stay stream-copy only: $forbiddenFlag" }
}
Require-Text $cutterProfile "--disable-avfilter" "Cutter should disable libavfilter entirely."
Require-Text $cutterProfile "--disable-swscale" "Cutter should disable libswscale entirely."
Require-Text $cutterProfile "--disable-swresample" "Cutter should disable libswresample entirely."
Require-Text $cutterProfile "--enable-demuxer=mov" "Cutter must read MP4/MOV."
Require-Text $cutterProfile "--enable-muxer=mp4" "Cutter must write MP4."
Require-Text $cutterReadme "keyframe" "Cutter profile docs must explain keyframe alignment."

Require-Text $packer "__FFMPEG_JS_GZIP_BASE64__" "Single-HTML packer is missing the JS payload token."
Require-Text $packer "__FFMPEG_WASM_GZIP_BASE64__" "Single-HTML packer is missing the Wasm payload token."
Require-Text $template "BrowserFFmpeg.loadEmbedded" "Video single-HTML template must use the embedded runtime."
Require-Text $cutterTemplate "BrowserFFmpeg.losslessVideoCutterArgs" "Cutter single-HTML demo must exercise the cutter runtime."
Require-Text $cutterTemplate "__FFMPEG_WASM_GZIP_BASE64__" "Cutter single-HTML demo must embed the Wasm payload."

Require-Text $smokeTemplate "__SMOKE_TEST_BODY__" "Generic smoke template must accept a profile test body."
Require-Text $smokeTemplate "SMOKE_TEST_PASS" "Smoke test PASS sentinel is missing."
Require-Text $smokePacker 'tests/smoke-tests/${PROFILE}.js' "Smoke-test packer must load profile-specific test logic."
Require-Text $videoSmoke "BrowserFFmpeg.videoCompressorArgs" "Video smoke test must run the actual transcoder."
Require-Text $cutterSmoke "BrowserFFmpeg.losslessVideoCutterArgs" "Cutter smoke test must run the actual cutter."
Require-Text $cutterSmoke 'output.byteLength >= input.byteLength' "Cutter smoke test must prove the range reduced output size."
Require-Text $cutterSmoke ' actual-start=' "Cutter smoke test must verify keyframe alignment reporting."
Require-Text $cutterSmoke 'keyframe-aligned=yes' "Cutter smoke test must require an explicitly keyframe-aligned result."
Require-Text $cutterSmoke 'workerfs: true' "Cutter smoke test must exercise the WORKERFS large-input path."
Require-Text $windowsBuild "smoke-test.ps1" "Windows build must run the browser smoke test automatically."
Require-Text $unixBuild "smoke-test.sh" "Unix/CI build must run the browser smoke test automatically."
Require-Text $smokeWindows "--remote-debugging-port=0" "Smoke runner must launch a real headless browser with DevTools enabled."
Require-Text $smokeWindows "SMOKE_TEST_PASS_bytes" "Smoke runner must wait for the browser PASS sentinel."

$fixtureBytes = [IO.File]::ReadAllBytes($smokeFixture)
if ($fixtureBytes.Length -lt 1024) { throw "Smoke input MP4 is unexpectedly small." }
if ([Text.Encoding]::ASCII.GetString($fixtureBytes, 4, 4) -ne "ftyp") { throw "Smoke input fixture is not an MP4 file." }

$windowsBuildText = [IO.File]::ReadAllText($windowsBuild)
$unixBuildText = [IO.File]::ReadAllText($unixBuild)
if ($windowsBuildText.Contains('"--builder", "default"')) { throw "Windows build must not force a Buildx builder across Docker contexts." }
if ($unixBuildText -match '--builder[ =]+default') { throw "Unix build must not force a Buildx builder across Docker contexts." }
Require-Text $windowsBuild '"buildx", "build"' "Windows build must use Buildx."
Require-Text $unixBuild "docker buildx build" "Unix build must use Buildx."
if ($null -ne $gitattributes) {
  Require-Text $gitattributes "*.sh text eol=lf" "Shell scripts must stay LF across Windows clones."
  Require-Text $gitattributes "*.bat text eol=crlf" "Windows batch launchers must use CRLF."
  Require-Text $gitattributes "*.mp4 binary" "Smoke fixture must be marked binary."
}

$versionsText = [IO.File]::ReadAllText($versions)
if ($versionsText -notmatch '(?m)^BUILDER_VERSION=1\.1\.0$') { throw "Builder version must be 1.1.0." }
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
Require-Text $dockerfile "FROM scratch AS export-no-x264" "Dockerfile must expose a no-x264 export target."
Require-Text $dockerfile "FROM scratch AS export-with-x264" "Dockerfile must expose an x264 export target."
Require-Text $unixBuild '0) EXPORT_TARGET="export-no-x264"' "Unix build must skip x264 for profiles that do not use it."
Require-Text $unixBuild '1) EXPORT_TARGET="export-with-x264"' "Unix build must select x264 only when required."
Require-Text $windowsBuild '"export-no-x264"' "Windows build must support the no-x264 Docker target."
Require-Text $windowsBuild '"export-with-x264"' "Windows build must support the x264 Docker target."

Require-Text $thirdParty 'generated `ffmpeg.wasm`' "Third-party notice must distinguish generated Wasm from the MIT builder source."
Require-Text $thirdParty "lossless-video-cutter" "Third-party notice must explain cutter x264 usage."
Require-Text $thirdParty "GPL-2.0-or-later" "Third-party notice must state video-compressor core licensing."
Require-Text $thirdParty "LGPL-2.1-or-later" "Third-party notice must state cutter core licensing."
Require-Text $licenseIndex "FFmpeg-COPYING.GPLv2" "License index must document FFmpeg GPL license packaging."
Require-Text $licenseIndex "FFmpeg-COPYING.LGPLv2.1" "License index must document FFmpeg LGPL license packaging."
Require-Text $licenseDoc "same GitHub Release" "Licensing docs must keep binaries and corresponding source together."
Require-Text $readme "lossless-video-cutter" "Japanese README must document the cutter profile."
Require-Text $readme "BrowserFFmpeg.losslessVideoCutterArgs" "Japanese README must document the cutter browser helper."
Require-Text $readmeEn 'does **not** relicense generated `ffmpeg.wasm`' "English README must clearly scope the root MIT license."
Require-Text $releaseDoc "git tag -a v1.1.0" "Release documentation must include the v1.1.0 tag procedure."

Require-Text $releaseScript 'RELEASE_PROFILES=(video-compressor lossless-video-cutter)' "Release packer must include both release profiles."
Require-Text $releaseScript 'fetch_exact "FFmpeg"' "Release packer must fetch exact FFmpeg source."
Require-Text $releaseScript 'fetch_exact "x264"' "Release packer must fetch exact x264 source."
Require-Text $releaseScript 'fetch_exact "Emscripten"' "Release packer must fetch exact Emscripten source."
Require-Text $releaseScript 'PROFILE_USE_X264' "Release bundle must make x264 notices profile-specific."
Require-Text $releaseScript 'PROFILE_BINARY_LICENSE' "Release bundle must choose the FFmpeg license text per profile."
Require-Text $releaseScript 'COPYING.LGPLv2.1' "Release bundle must support LGPL profile licensing."
Require-Text $releaseScript "ffmpeg-wasm-sources-v" "Release packer must create a corresponding-source archive."
Require-Text $releaseScript "sha256sum" "Release packer must generate SHA-256 checksums."

Require-Text $buildWorkflow "lossless-video-cutter" "Main CI must build and smoke-test the cutter."
Require-Text $buildWorkflow "video-compressor" "Main CI must keep testing video compressor."
Require-Text $releaseWorkflow 'tags:' "Release workflow must be tag-driven."
Require-Text $releaseWorkflow 'test "${GITHUB_REF_NAME}" = "v${BUILDER_VERSION}"' "Release workflow must verify tag/version equality."
Require-Text $releaseWorkflow "./build.sh lossless-video-cutter" "Release workflow must smoke-test cutter before publishing."
Require-Text $releaseWorkflow "./build.sh video-compressor" "Release workflow must smoke-test video compressor before publishing."
Require-Text $releaseWorkflow "ffmpeg-wasm-lossless-video-cutter" "Release workflow must publish the cutter binary bundle."
Require-Text $releaseWorkflow "--verify-tag" "Release creation must refuse an unpushed/missing tag."
Require-Text $releaseWorkflow "contents: write" "Release workflow needs explicit contents:write permission."

$node = Get-Command node -ErrorAction SilentlyContinue
if ($node) {
  & node --check $runtime
  if ($LASTEXITCODE -ne 0) { throw "JavaScript syntax check failed: runtime/browser-ffmpeg.js" }
  foreach ($smokeBody in @($videoSmoke, $cutterSmoke)) {
    $wrapped = "async function __smoke(){`n" + [IO.File]::ReadAllText($smokeBody) + "`n}"
    $temp = Join-Path ([IO.Path]::GetTempPath()) ("ffmpeg-smoke-" + [guid]::NewGuid().ToString("N") + ".js")
    [IO.File]::WriteAllText($temp, $wrapped)
    try {
      & node --check $temp
      if ($LASTEXITCODE -ne 0) { throw "JavaScript syntax check failed: $smokeBody" }
    } finally {
      Remove-Item -Force $temp -ErrorAction SilentlyContinue
    }
  }
}

Write-Host "[OK] Repository checks passed." -ForegroundColor Green
