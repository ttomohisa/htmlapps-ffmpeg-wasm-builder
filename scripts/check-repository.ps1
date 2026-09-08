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
$speedRunner = Require-File "runners/video-speed-changer.c"
$cutterRunner = Require-File "runners/lossless-video-cutter.c"
$inspectorRunner = Require-File "runners/media-inspector.c"
$contactRunner = Require-File "runners/video-contact-sheet.c"
$videoProfile = Require-File "profiles/video-compressor/ffmpeg.flags"
$videoProfileEnv = Require-File "profiles/video-compressor/profile.env"
$speedProfile = Require-File "profiles/video-speed-changer/ffmpeg.flags"
$speedProfileEnv = Require-File "profiles/video-speed-changer/profile.env"
$speedReadme = Require-File "profiles/video-speed-changer/README.md"
$speedTemplate = Require-File "profiles/video-speed-changer/single-html/template.html"
$cutterProfile = Require-File "profiles/lossless-video-cutter/ffmpeg.flags"
$cutterProfileEnv = Require-File "profiles/lossless-video-cutter/profile.env"
$cutterReadme = Require-File "profiles/lossless-video-cutter/README.md"
$cutterTemplate = Require-File "profiles/lossless-video-cutter/single-html/template.html"
$inspectorProfile = Require-File "profiles/media-inspector/ffmpeg.flags"
$inspectorProfileEnv = Require-File "profiles/media-inspector/profile.env"
$inspectorReadme = Require-File "profiles/media-inspector/README.md"
$inspectorTemplate = Require-File "profiles/media-inspector/single-html/template.html"
$contactProfile = Require-File "profiles/video-contact-sheet/ffmpeg.flags"
$contactProfileEnv = Require-File "profiles/video-contact-sheet/profile.env"
$contactReadme = Require-File "profiles/video-contact-sheet/README.md"
$contactTemplate = Require-File "profiles/video-contact-sheet/single-html/template.html"
$gifRunner = Require-File "runners/video-to-gif.c"
$webpRunner = Require-File "runners/video-to-webp.c"
$animationRunner = Require-File "runners/video-to-animation-common.inc"
$gifProfile = Require-File "profiles/video-to-gif/ffmpeg.flags"
$gifProfileEnv = Require-File "profiles/video-to-gif/profile.env"
$gifReadme = Require-File "profiles/video-to-gif/README.md"
$gifTemplate = Require-File "profiles/video-to-gif/single-html/template.html"
$webpProfile = Require-File "profiles/video-to-webp/ffmpeg.flags"
$webpProfileEnv = Require-File "profiles/video-to-webp/profile.env"
$webpReadme = Require-File "profiles/video-to-webp/README.md"
$webpTemplate = Require-File "profiles/video-to-webp/single-html/template.html"
$template = Require-File "profiles/video-compressor/single-html/template.html"
$packer = Require-File "scripts/pack-single-html.ps1"
$smokePacker = Require-File "scripts/pack-smoke-test.sh"
$smokeTemplate = Require-File "tests/smoke-test.template.html"
$videoSmoke = Require-File "tests/smoke-tests/video-compressor.js"
$speedSmoke = Require-File "tests/smoke-tests/video-speed-changer.js"
$cutterSmoke = Require-File "tests/smoke-tests/lossless-video-cutter.js"
$inspectorSmoke = Require-File "tests/smoke-tests/media-inspector.js"
$contactSmoke = Require-File "tests/smoke-tests/video-contact-sheet.js"
$gifSmoke = Require-File "tests/smoke-tests/video-to-gif.js"
$webpSmoke = Require-File "tests/smoke-tests/video-to-webp.js"
$filterRunner = Require-File "runners/ffmpeg-filter-builder.c"
$filterProfile = Require-File "profiles/ffmpeg-filter-builder/ffmpeg.flags"
$filterProfileEnv = Require-File "profiles/ffmpeg-filter-builder/profile.env"
$filterReadme = Require-File "profiles/ffmpeg-filter-builder/README.md"
$filterSmoke = Require-File "tests/smoke-tests/ffmpeg-filter-builder.js"
$filterLauncher = Require-File "build-ffmpeg-filter-builder.bat"
$libwebpBuild = Require-File "scripts/build-libwebp.sh"
$libvpxBuild = Require-File "scripts/build-libvpx.sh"
$libopusBuild = Require-File "scripts/build-libopus.sh"
$smokeFixture = Require-File "tests/fixtures/smoke-input.mp4"
$rotatedSmokeFixture = Require-File "tests/fixtures/smoke-rotated.mp4"
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
Require-Text $buildScript 'PROFILE_USE_LIBVPX' "Build must make libvpx profile-specific."
Require-Text $buildScript 'PROFILE_USE_LIBOPUS' "Build must make Opus profile-specific."
Require-Text $libvpxBuild "--enable-vp9-encoder" "libvpx build must keep the VP9 encoder."
Require-Text $libvpxBuild "--disable-vp9-decoder" "libvpx build must stay encoder-only."
Require-Text $libvpxBuild "--enable-small" "libvpx build must favor compact output."
Require-Text $libopusBuild "OPUS_DRED=OFF" "Opus build must keep DRED out of the compact profile."
Require-Text $libopusBuild "OPUS_OSCE=OFF" "Opus build must keep OSCE out of the compact profile."
Require-Text $buildScript 'PROFILE_USE_LIBWEBP' "Build must make libwebp profile-specific."
Require-Text $buildScript 'PROFILE_USE_WORKERFS' "Build must make WORKERFS profile-specific."
Require-Text $buildScript '-lworkerfs.js' "WORKERFS profiles must explicitly link Emscripten WORKERFS."
Require-Text $buildScript 'WORKERFS' "WORKERFS must be exported to the browser runtime when enabled."
Require-Text $buildScript "--disable-pthreads" "Single-thread WASM builds must explicitly disable pthreads."
Require-Text $buildScript "--enable-pthreads" "Explicit multi-thread WASM builds must enable pthreads."
Require-Text $buildScript 'THREADING_MODE' "WASM build must select threading per profile variant."
Require-Text $buildScript 'PTHREAD_POOL_SIZE' "Multi-thread builds must declare a bounded pthread pool."
Require-Text $buildScript "--disable-programs" "WASM build must not link the upstream ffmpeg CLI."
Require-Text $buildScript "-sEXPORT_NAME=createFFmpegCore" "WASM factory name must stay stable."
Require-Text $buildScript "mainScriptUrlOrBlob" "WASM build must allow Blob-hosted main script URLs for pthread workers."
Require-Text $buildScript "-sUSE_ZLIB=1" "Profiles that request zlib must use the pinned Emscripten zlib system port."
Require-Text $buildScript '"schemaVersion": 8' "Manifest schema must include threading-aware metadata."
Require-Text $buildScript '"requiresSharedArrayBuffer":' "Manifest must describe SharedArrayBuffer requirements."
Require-Text $buildScript '"requiresCrossOriginIsolation":' "Manifest must describe cross-origin isolation requirements."
Require-Text $buildScript '"decoderThreadCount":' "Manifest must describe decoder thread usage."
Require-Text $buildScript '"encoderThreadCount":' "Manifest must describe encoder thread usage."
Require-Text $buildScript '"x264LookaheadThreadCount":' "Manifest must describe the x264 lookahead worker budget."
Require-Text $buildScript '"pthreadWorkerStrategy":' "Manifest must describe how pthread workers load the main script."
Require-Text $buildScript 'Unexpected legacy pthread worker asset' "Build must reject obsolete separate pthread worker assets."
Require-Text $buildScript '"x264Linked":' "Manifest must state whether x264 is linked."
Require-Text $buildScript '"libvpxLinked":' "Manifest must state whether libvpx is linked."
Require-Text $buildScript '"libopusLinked":' "Manifest must state whether Opus is linked."
Require-Text $buildScript '"libwebpLinked":' "Manifest must state whether libwebp is linked."

Require-Text $runtime "instantiateWasm" "Browser runtime must instantiate transferred Wasm bytes directly."
Require-Text $runtime "new Blob([coreJsText" "Browser runtime must combine generated core JS and Worker body into one Blob."
Require-Text $runtime "losslessVideoCutterArgs" "Browser runtime must expose Lossless Video Cutter args."
Require-Text $runtime "mediaInspectorArgs" "Browser runtime must expose Media Inspector args."
Require-Text $runtime "videoContactSheetArgs" "Browser runtime must expose Video Contact Sheet args."
Require-Text $runtime "videoToGifArgs" "Browser runtime must expose animated GIF args."
Require-Text $runtime "videoToWebpArgs" "Browser runtime must expose animated WebP args."
Require-Text $runtime "decodePpmOutput" "Browser runtime must parse Video Contact Sheet PPM output."
Require-Text $runtime "decodeJsonOutput" "Browser runtime must decode structured JSON outputs."
Require-Text $runtime "mountWorkerFiles" "Browser runtime must support Blob/File-backed WORKERFS mounts."
Require-Text $runtime "file.workerfs === true" "Browser runtime must keep WORKERFS inputs out of the ArrayBuffer/MEMFS path."
Require-Text $runtime "window.BrowserFFmpeg" "Browser runtime must expose BrowserFFmpeg."
Require-Text $runtime "mainScriptUrlOrBlob" "Browser runtime must pass the Blob-hosted Emscripten main script to pthread workers."
Require-Text $runtime "new Blob([coreJsText]" "Browser runtime must reuse embedded core JS as the pthread worker program."
Require-Text $runtime 'threading === "multi-thread"' "Browser runtime must explicitly gate pthread behavior."
Require-Text $runtime "crossOriginIsolated" "Multi-thread browser runtime must reject non-isolated hosting."
Require-Text $runtime "ffmpegFilterBuilderArgs" "Browser runtime must expose FFmpeg Filter Builder args."
$runtimeText = [IO.File]::ReadAllText($runtime)
if ($runtimeText.Contains("pthreadWorkerJsText") -or $runtimeText.Contains("pthreadWorkerJsUrl")) { throw "Browser runtime must not require a separate pthread worker asset on Emscripten 6.x." }

$videoRunnerText = [IO.File]::ReadAllText($videoRunner)
if ($videoRunnerText -match 'pthread_(create|join|mutex|cond)') { throw "Video runner must not call pthread APIs." }
Require-Text $videoRunner '#define RUNNER_VERSION "1.6.0"' "Video runner version must be 1.6.0."
Require-Text $videoProfile "--enable-encoder=libvpx_vp9" "FFmpeg configure must use the libvpx_vp9 component name."
$videoProfileFlagsText = [IO.File]::ReadAllText($videoProfile)
if ($videoProfileFlagsText.Contains("--enable-encoder=libvpx-vp9")) { throw "FFmpeg configure must not use the runtime codec name libvpx-vp9." }
Require-Text $videoRunner "libvpx-vp9" "Video runner must support VP9 output."
Require-Text $videoRunner "libopus" "Video runner must support Opus audio."
Require-Text $videoRunner "--inspect-output" "Video runner must expose measured source inspection."
Require-Text $videoRunner "AV_PKT_DATA_DISPLAYMATRIX" "Video runner must read rotation metadata."
Require-Text $videoRunner "get_display_rotation_degrees" "Video runner must report display rotation with ffprobe semantics."
Require-Text $videoRunner "get_autorotate_degrees" "Video runner must keep FFmpeg autorotate angle normalization separate from reported rotation."
Require-Text $videoRunner "append_autorotate_filter" "Video runner must apply display rotation to output pixels."
Require-Text $videoSmoke "displayWidth !== 96" "Video smoke test must verify display-matrix dimensions."
Require-Text $videoSmoke "H.264 autorotation did not produce portrait pixels" "Video smoke test must verify H.264 autorotation."
Require-Text $videoSmoke "VP9 autorotation did not produce portrait pixels" "Video smoke test must verify VP9 autorotation."
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
Require-Text $cutterRunner '#define RUNNER_VERSION "1.5.0"' "Cutter runner version must remain 1.5.0 when unchanged."
Require-Text $cutterRunner "av_seek_frame" "Cutter must seek to a keyframe."
Require-Text $cutterRunner "AV_PKT_FLAG_KEY" "Cutter must anchor output to a keyframe."
Require-Text $cutterRunner "avcodec_parameters_copy" "Cutter must stream-copy codec parameters."
Require-Text $cutterRunner "av_packet_rescale_ts" "Cutter must rescale packet timestamps."
Require-Text $cutterRunner "av_interleaved_write_frame" "Cutter must remux packets without decoding."
Require-Text $cutterRunner "actual-start=" "Cutter must report the keyframe-aligned actual start."
foreach ($forbiddenApi in @("avcodec_send_packet", "avcodec_receive_frame", "avcodec_send_frame", "avcodec_receive_packet", "avfilter_graph_alloc")) {
  if ($cutterRunnerText.Contains($forbiddenApi)) { throw "Lossless cutter must not decode/encode/filter: $forbiddenApi" }
}

$inspectorRunnerText = [IO.File]::ReadAllText($inspectorRunner)
if ($inspectorRunnerText -match 'pthread_(create|join|mutex|cond)') { throw "Media Inspector runner must not call pthread APIs." }
Require-Text $inspectorRunner '#define RUNNER_VERSION "1.5.0"' "Media Inspector runner version must remain 1.5.0 when unchanged."
Require-Text $inspectorRunner "avformat_open_input" "Media Inspector must open media through libavformat."
Require-Text $inspectorRunner "avformat_find_stream_info" "Media Inspector must discover stream information."
Require-Text $inspectorRunner "AV_PKT_DATA_DISPLAYMATRIX" "Media Inspector must report rotation/display matrix data."
Require-Text $inspectorRunner "AV_PKT_DATA_MASTERING_DISPLAY_METADATA" "Media Inspector must inspect HDR mastering metadata."
Require-Text $inspectorRunner "AV_PKT_DATA_CONTENT_LIGHT_LEVEL" "Media Inspector must inspect HDR content-light metadata."
Require-Text $inspectorRunner "json_chapters" "Media Inspector must include chapters in the JSON report."
foreach ($forbiddenApi in @("avcodec_send_packet", "avcodec_receive_frame", "avcodec_send_frame", "avcodec_receive_packet", "av_interleaved_write_frame", "avfilter_graph_alloc")) {
  if ($inspectorRunnerText.Contains($forbiddenApi)) { throw "Media Inspector must stay inspect-only: $forbiddenApi" }
}

$contactRunnerText = [IO.File]::ReadAllText($contactRunner)
if ($contactRunnerText -match 'pthread_(create|join|mutex|cond)') { throw "Video Contact Sheet runner must not call pthread APIs." }
Require-Text $contactRunner '#define RUNNER_VERSION "1.5.0"' "Video Contact Sheet runner version must remain 1.5.0 when unchanged."
Require-Text $contactRunner "av_seek_frame" "Video Contact Sheet must seek between sample points."
Require-Text $contactRunner "avcodec_send_packet" "Video Contact Sheet must decode selected frames."
Require-Text $contactRunner "avcodec_receive_frame" "Video Contact Sheet decode loop is missing."
Require-Text $contactRunner "sws_scale" "Video Contact Sheet must convert decoded frames to RGB."
Require-Text $contactRunner 'fprintf(out, "P6\n%d %d\n255\n"' "Video Contact Sheet must write a P6 PPM output."
Require-Text $contactRunner "metadata_output_path" "Video Contact Sheet must support optional JSON sample metadata."
foreach ($forbiddenApi in @("avcodec_send_frame", "avcodec_receive_packet", "av_interleaved_write_frame", "avfilter_graph_alloc")) {
  if ($contactRunnerText.Contains($forbiddenApi)) { throw "Video Contact Sheet must stay decode-only: $forbiddenApi" }
}

Require-Text $videoProfileEnv "PROFILE_USE_X264=1" "Video profile must link x264."
Require-Text $speedProfileEnv "PROFILE_USE_X264=1" "Video Speed Changer profile must link x264."
Require-Text $speedProfile "--enable-filter=setpts" "Video Speed Changer must enable setpts."
Require-Text $speedProfile "--enable-filter=atempo" "Video Speed Changer must enable atempo."
Require-Text $speedProfile "--enable-filter=asetrate" "Video Speed Changer must enable asetrate for pitch-shifting audio."
Require-Text $speedRunner "--rate" "Video Speed Changer runner must expose a bounded rate option."
Require-Text $runtime "videoSpeedChangerArgs" "Browser runtime must expose Video Speed Changer helper."
$speedRunnerText = [IO.File]::ReadAllText($speedRunner)
if ($speedRunnerText -match 'pthread_(create|join|mutex|cond)') { throw "Video Speed Changer runner must not call pthread APIs." }
Require-Text $speedRunner '#define RUNNER_VERSION "1.8.0"' "Video Speed Changer runner remains 1.8.0; Builder v1.9.x keeps the Video Speed Changer runner unchanged while evolving the Filter Builder runtime architecture."
Require-Text $speedRunner "setpts=PTS/" "Video Speed Changer runner must adjust video timestamps."
Require-Text $speedRunner "stream->enc_ctx->time_base = input_stream->time_base" "Video Speed Changer must preserve a high-resolution input time base when available."
Require-Text $speedRunner "frame_rate = av_mul_q(source_frame_rate, speed_q)" "Video Speed Changer must scale encoder frame rate with playback rate to avoid duplicate PTS."
Require-Text $runtime "Recent FFmpeg log" "Browser runtime must preserve recent FFmpeg logs on runner failure."
Require-Text $runtime "options.signal" "Browser runtime must accept AbortSignal for cancellable runs."
Require-Text $runtime "activeRuns" "Browser runtime dispose must terminate active runs."
Require-Text $runtime "videoSpeedChangerInspectArgs" "Browser runtime must expose Video Speed Changer inspect helper."
Require-Text $speedSmoke 'append("case=" + label + " start")' "Video Speed Changer smoke test must identify the failing rate/audio mode."
Require-Text $speedRunner "append_atempo_chain" "Video Speed Changer runner must chain pitch-preserving atempo filters."
Require-Text $speedRunner "asetrate=%d" "Video Speed Changer runner must support pitch-shifting audio."
Require-Text $runtime "options.preservePitch === false" "Video Speed Changer browser helper must expose pitch-shifting mode."
Require-Text $speedSmoke "0.25, 0.5, 0.75, 1.0, 1.25, 1.5, 2.0, 3.0, 4.0" "Video Speed Changer smoke test must cover the full preset rate range."
Require-Text $speedSmoke "preservePitch: false" "Video Speed Changer smoke test must cover pitch-shifting audio."
Require-Text $speedSmoke "noAudio: true" "Video Speed Changer smoke test must cover audio removal."
Require-Text $speedSmoke "source-no-audio" "Video Speed Changer smoke test must cover a source video without audio."
Require-Text $speedSmoke "case=abort start" "Video Speed Changer smoke test must cover AbortSignal cancellation."
Require-Text $speedSmoke "videoSpeedChangerInspectArgs" "Video Speed Changer smoke test must cover media inspection."
Require-Text $thirdParty "video-speed-changer" "Third-party notice must explain Video Speed Changer licensing."
Require-Text $readme "video-speed-changer" "Japanese README must document the Video Speed Changer profile."
Require-Text $readme "BrowserFFmpeg.videoSpeedChangerArgs" "Japanese README must document the Video Speed Changer browser helper."
Require-Text $readmeEn "video-speed-changer" "English README must document the Video Speed Changer profile."
Require-Text $videoProfileEnv "PROFILE_USE_LIBVPX=1" "Video profile must link libvpx for VP9."
Require-Text $videoProfileEnv "PROFILE_USE_LIBOPUS=1" "Video profile must link Opus for WebM audio."
Require-Text $videoProfileEnv "PROFILE_USE_WORKERFS=1" "Video profile must use WORKERFS to avoid copying full media files into MEMFS."
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

Require-Text $inspectorProfileEnv "PROFILE_USE_X264=0" "Media Inspector must not link x264."
Require-Text $inspectorProfileEnv "PROFILE_USE_WORKERFS=1" "Media Inspector must use WORKERFS for large File/Blob inputs."
Require-Text $inspectorProfileEnv 'PROFILE_BINARY_LICENSE="LGPL-2.1-or-later"' "Media Inspector should remain LGPL without GPL-only components."
Require-Text $inspectorProfileEnv "libavformat/libavformat.a" "Media Inspector must link libavformat."
Require-Text $inspectorProfileEnv "libavcodec/libavcodec.a" "Media Inspector must link libavcodec descriptor/parser APIs."
Require-Text $inspectorProfileEnv "libavutil/libavutil.a" "Media Inspector must link libavutil."
$inspectorProfileEnvText = [IO.File]::ReadAllText($inspectorProfileEnv)
foreach ($heavy in @("libavfilter/libavfilter.a", "libswscale/libswscale.a", "libswresample/libswresample.a")) {
  if ($inspectorProfileEnvText.Contains($heavy)) { throw "Media Inspector should not link heavy processing library: $heavy" }
}
$inspectorFlagsText = [IO.File]::ReadAllText($inspectorProfile)
foreach ($forbiddenFlag in @("--enable-decoder=", "--enable-encoder=", "--enable-muxer=", "--enable-filter=", "--enable-libx264", "--enable-gpl")) {
  if ($inspectorFlagsText.Contains($forbiddenFlag)) { throw "Media Inspector flags must stay inspect-only: $forbiddenFlag" }
}
Require-Text $inspectorProfile "--disable-avfilter" "Media Inspector should disable libavfilter entirely."
Require-Text $inspectorProfile "--disable-swscale" "Media Inspector should disable libswscale entirely."
Require-Text $inspectorProfile "--disable-swresample" "Media Inspector should disable libswresample entirely."
Require-Text $inspectorProfile "--enable-demuxer=mov" "Media Inspector must read MP4/MOV."
Require-Text $inspectorProfile "--enable-demuxer=matroska" "Media Inspector must read MKV/WebM."
Require-Text $inspectorProfile "--enable-parser=h264" "Media Inspector must parse H.264 stream headers."
Require-Text $inspectorProfile "--enable-parser=hevc" "Media Inspector must parse HEVC stream headers."
Require-Text $inspectorReadme "Media Doctor" "Media Inspector docs must explain the browser-diagnosis layer."

Require-Text $contactProfileEnv "PROFILE_USE_X264=0" "Video Contact Sheet must not link x264."
Require-Text $contactProfileEnv "PROFILE_USE_WORKERFS=1" "Video Contact Sheet must use WORKERFS for large File/Blob inputs."
Require-Text $contactProfileEnv 'PROFILE_BINARY_LICENSE="LGPL-2.1-or-later"' "Video Contact Sheet should remain LGPL without GPL-only components."
Require-Text $contactProfileEnv "libavformat/libavformat.a" "Video Contact Sheet must link libavformat."
Require-Text $contactProfileEnv "libavcodec/libavcodec.a" "Video Contact Sheet must link libavcodec."
Require-Text $contactProfileEnv "libswscale/libswscale.a" "Video Contact Sheet must link libswscale for RGB conversion."
Require-Text $contactProfileEnv "libavutil/libavutil.a" "Video Contact Sheet must link libavutil."
$contactProfileEnvText = [IO.File]::ReadAllText($contactProfileEnv)
foreach ($heavy in @("libavfilter/libavfilter.a", "libswresample/libswresample.a")) {
  if ($contactProfileEnvText.Contains($heavy)) { throw "Video Contact Sheet should not link unnecessary processing library: $heavy" }
}
$contactFlagsText = [IO.File]::ReadAllText($contactProfile)
foreach ($forbiddenFlag in @("--enable-encoder=", "--enable-muxer=", "--enable-filter=", "--enable-libx264", "--enable-gpl")) {
  if ($contactFlagsText.Contains($forbiddenFlag)) { throw "Video Contact Sheet flags must stay decode-only: $forbiddenFlag" }
}
Require-Text $contactProfile "--disable-avfilter" "Video Contact Sheet should disable libavfilter entirely."
Require-Text $contactProfile "--disable-swresample" "Video Contact Sheet should disable libswresample entirely."
Require-Text $contactProfile "--enable-decoder=h264" "Video Contact Sheet must decode H.264."
Require-Text $contactProfile "--enable-decoder=hevc" "Video Contact Sheet must decode HEVC/Pixel hvc1."
Require-Text $contactProfile "--enable-decoder=av1" "Video Contact Sheet must decode AV1."
Require-Text $contactProfile "--enable-demuxer=mov" "Video Contact Sheet must read MP4/MOV."
Require-Text $contactProfile "--enable-demuxer=matroska" "Video Contact Sheet must read MKV/WebM."
Require-Text $contactReadme 'Pixel `hvc1`' "Video Contact Sheet docs must document Pixel HEVC support."

Require-Text $animationRunner '#define RUNNER_VERSION "1.5.0"' "Animation runner version must remain 1.5.0 when unchanged."
Require-Text $animationRunner 'palettegen=max_colors=' "GIF runner must generate a palette in a first pass."
Require-Text $animationRunner 'paletteuse=dither=' "GIF runner must apply the generated palette."
Require-Text $animationRunner 'avcodec_find_encoder_by_name("libwebp_anim")' "Animated WebP runner must use FFmpeg libwebp_anim."
Require-Text $animationRunner 'av_seek_frame' "Animation runner must seek to trim starts."
Require-Text $animationRunner '"--crop-x"' "Animation runner must implement crop rectangles."
Require-Text $animationRunner 'crop=%d:%d:%d:%d' "Animation runner filter graph must crop before resize."
Require-Text $animationRunner 'WORKERFS' "Animation profile docs/source should preserve large-file WORKERFS intent."
Require-Text $gifProfile '--enable-encoder=gif' "GIF profile must enable only the GIF encoder it needs."
Require-Text $gifProfile '--enable-filter=crop' "GIF profile must enable crop."
Require-Text $gifProfile '--enable-filter=palettegen' "GIF profile must enable palettegen."
Require-Text $gifProfile '--enable-filter=paletteuse' "GIF profile must enable paletteuse."
Require-Text $gifProfileEnv 'PROFILE_USE_LIBWEBP=0' "GIF profile must not link libwebp."
Require-Text $webpProfile '--enable-filter=crop' "WebP profile must enable crop."
Require-Text $webpProfile '--enable-libwebp' "WebP profile must enable libwebp."
Require-Text $webpProfile '--enable-encoder=libwebp_anim' "WebP profile must enable the animation encoder."
Require-Text $webpProfileEnv 'PROFILE_USE_LIBWEBP=1' "WebP profile must link libwebp."
Require-Text $libwebpBuild '--disable-threading' "libwebp build must remain single-threaded."
Require-Text $libwebpBuild '--enable-libwebpmux' "libwebp animation build needs mux support."
Require-Text $gifTemplate 'BrowserFFmpeg.videoToGifArgs' "GIF single-HTML demo must exercise the GIF runtime helper."
Require-Text $webpTemplate 'BrowserFFmpeg.videoToWebpArgs' "WebP single-HTML demo must exercise the WebP runtime helper."

Require-Text $packer "__FFMPEG_JS_GZIP_BASE64__" "Single-HTML packer is missing the JS payload token."
Require-Text $packer "__FFMPEG_WASM_GZIP_BASE64__" "Single-HTML packer is missing the Wasm payload token."
Require-Text $template "BrowserFFmpeg.loadEmbedded" "Video single-HTML template must use the embedded runtime."
Require-Text $cutterTemplate "BrowserFFmpeg.losslessVideoCutterArgs" "Cutter single-HTML demo must exercise the cutter runtime."
Require-Text $cutterTemplate "__FFMPEG_WASM_GZIP_BASE64__" "Cutter single-HTML demo must embed the Wasm payload."
Require-Text $inspectorTemplate "BrowserFFmpeg.mediaInspectorArgs" "Media Inspector single-HTML demo must exercise the inspector runtime."
Require-Text $inspectorTemplate "__FFMPEG_WASM_GZIP_BASE64__" "Media Inspector single-HTML demo must embed the Wasm payload."
Require-Text $contactTemplate "BrowserFFmpeg.videoContactSheetArgs" "Video Contact Sheet demo must exercise the contact-sheet runtime."
Require-Text $contactTemplate "BrowserFFmpeg.decodePpmOutput" "Video Contact Sheet demo must parse the PPM output."
Require-Text $contactTemplate "__FFMPEG_WASM_GZIP_BASE64__" "Video Contact Sheet demo must embed the Wasm payload."

Require-Text $smokeTemplate "__SMOKE_TEST_BODY__" "Generic smoke template must accept a profile test body."
Require-Text $smokeTemplate "SMOKE_TEST_PASS" "Smoke test PASS sentinel is missing."
Require-Text $smokePacker 'tests/smoke-tests/${PROFILE}.js' "Smoke-test packer must load profile-specific test logic."
Require-Text $videoSmoke "BrowserFFmpeg.videoCompressorArgs" "Video smoke test must run the actual transcoder."
Require-Text $cutterSmoke "BrowserFFmpeg.losslessVideoCutterArgs" "Cutter smoke test must run the actual cutter."
Require-Text $cutterSmoke 'output.byteLength >= input.byteLength' "Cutter smoke test must prove the range reduced output size."
Require-Text $cutterSmoke ' actual-start=' "Cutter smoke test must verify keyframe alignment reporting."
Require-Text $cutterSmoke 'keyframe-aligned=yes' "Cutter smoke test must require an explicitly keyframe-aligned result."
Require-Text $cutterSmoke 'workerfs: true' "Cutter smoke test must exercise the WORKERFS large-input path."
Require-Text $inspectorSmoke "BrowserFFmpeg.mediaInspectorArgs" "Media Inspector smoke test must run the actual inspector."
Require-Text $contactSmoke "BrowserFFmpeg.videoContactSheetArgs" "Video Contact Sheet smoke test must run the actual sampler."
Require-Text $contactSmoke "BrowserFFmpeg.decodePpmOutput" "Video Contact Sheet smoke test must validate the PPM output."
Require-Text $contactSmoke "workerfs: true" "Video Contact Sheet smoke test must exercise WORKERFS input."
Require-Text $contactSmoke "meta.samples.length !== 12" "Video Contact Sheet smoke test must verify all 12 sample timestamps."
Require-Text $gifSmoke "BrowserFFmpeg.videoToGifArgs" "GIF smoke test must run the actual animation runner."
Require-Text $gifSmoke 'crop:' "GIF smoke test must exercise crop."
Require-Text $gifSmoke '"GIF89a"' "GIF smoke test must validate the GIF signature."
Require-Text $gifSmoke '"NETSCAPE2.0"' "GIF smoke test must validate looping animation output."
Require-Text $webpSmoke "BrowserFFmpeg.videoToWebpArgs" "WebP smoke test must run the actual animation runner."
Require-Text $webpSmoke 'crop:' "WebP smoke test must exercise crop."
Require-Text $webpSmoke '"ANIM"' "WebP smoke test must validate the animation chunk."
Require-Text $webpSmoke '"ANMF"' "WebP smoke test must validate animation frames."
Require-Text $inspectorSmoke "BrowserFFmpeg.decodeJsonOutput" "Media Inspector smoke test must parse the structured report."
Require-Text $inspectorSmoke 'video.codec?.name !== "h264"' "Media Inspector smoke test must validate video codec reporting."
Require-Text $inspectorSmoke 'audio.audio?.sampleRate !== 48000' "Media Inspector smoke test must validate audio reporting."
Require-Text $inspectorSmoke 'workerfs: true' "Media Inspector smoke test must exercise the WORKERFS input path."
Require-Text $windowsBuild "smoke-test.ps1" "Windows build must run the browser smoke test automatically."
Require-Text $unixBuild "smoke-test.sh" "Unix/CI build must run the browser smoke test automatically."
Require-Text $smokeWindows "--remote-debugging-port=0" "Smoke runner must launch a real headless browser with DevTools enabled."
Require-Text $smokeWindows "SMOKE_TEST_PASS_(.*)" "Smoke runner must accept profile-specific browser PASS details."

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

Require-Text $filterProfileEnv 'PROFILE_THREADING_VARIANTS="single-thread,multi-thread"' "FFmpeg Filter Builder must ship both threading variants."
Require-Text $filterProfileEnv 'PROFILE_PTHREAD_POOL_SIZE=8' "FFmpeg Filter Builder must pin a pthread pool large enough for concurrent decode + encode."
Require-Text $filterProfileEnv 'PROFILE_DECODER_THREAD_COUNT=2' "FFmpeg Filter Builder must reserve a smaller decoder worker budget."
Require-Text $filterProfileEnv 'PROFILE_ENCODER_THREAD_COUNT=4' "FFmpeg Filter Builder must reserve four x264 encoder workers."
Require-Text $filterProfileEnv 'PROFILE_X264_LOOKAHEAD_THREAD_COUNT=1' "FFmpeg Filter Builder must explicitly budget the x264 lookahead worker."
Require-Text $filterProfileEnv 'PROFILE_USE_X264=1' "FFmpeg Filter Builder must provide H.264 output."
Require-Text $filterProfileEnv 'PROFILE_USE_ZLIB=1' "FFmpeg Filter Builder must explicitly enable the zlib system port for PNG input."
Require-Text $filterProfile '--enable-zlib' "FFmpeg Filter Builder must enable FFmpeg zlib support for PNG decoding."
Require-Text $filterProfileEnv 'CONFIG_ZLIB' "FFmpeg Filter Builder must assert zlib was enabled by configure."
Require-Text $filterProfileEnv 'PROFILE_USE_WORKERFS=1' "FFmpeg Filter Builder must use WORKERFS input."
foreach ($filterName in @("scale", "crop", "trim", "overlay", "amix", "loudnorm")) {
  Require-Text $filterProfile "--enable-filter=$filterName" "FFmpeg Filter Builder profile is missing a required filter."
}
Require-Text $filterRunner '#define RUNNER_VERSION "0.2.2"' "FFmpeg Filter Builder runner version must be 0.2.2 for fine-grained Filter Builder video timestamps."
Require-Text $filterRunner "--video-filter" "FFmpeg Filter Builder runner must accept compiled video filter chains."
Require-Text $filterRunner "--audio-filter" "FFmpeg Filter Builder runner must accept compiled audio filter chains."
Require-Text $filterRunner "--start-time" "FFmpeg Filter Builder runner must accept a relative preview start time."
Require-Text $filterRunner "--duration" "FFmpeg Filter Builder runner must accept a bounded preview duration."
Require-Text $filterRunner "typedef struct PacketMeasure" "FFmpeg Filter Builder runner must keep the media-inspection packet measurement type."
Require-Text $filterRunner "trim=start=" "FFmpeg Filter Builder runner must compile video time ranges through trim."
Require-Text $filterRunner "atrim=start=" "FFmpeg Filter Builder runner must keep audio aligned with ranged video output."
Require-Text $filterRunner "FFMPEG_WASM_PTHREADS" "FFmpeg Filter Builder runner must be threading-aware."
Require-Text $runtime "startTimeSeconds" "Filter Builder browser helper must expose ranged preview start time."
Require-Text $runtime "durationSeconds" "Filter Builder browser helper must expose bounded preview duration."
Require-Text $filterProfileEnv '"timeRangeRender":true' "Filter Builder manifest capabilities must advertise time-range rendering."
Require-Text $filterSmoke "scale=160:90" "FFmpeg Filter Builder smoke test must execute a real scale filter."
Require-Text $filterSmoke "durationSeconds: 0.1" "FFmpeg Filter Builder smoke test must exercise an early bounded range that finishes well before source EOF."
Require-Text $filterSmoke "report.duration" "FFmpeg Filter Builder smoke test must inspect and verify trimmed output duration."
Require-Text $filterSmoke "report.video.width !== 160" "FFmpeg Filter Builder smoke test must verify caller scale dimensions are preserved without maxWidth/maxHeight masking."
Require-Text $filterRunner "ret == AVERROR_EOF" "FFmpeg Filter Builder runner must treat filter-source EOF as a normal bounded-range completion."
Require-Text $filterRunner "probe_video_filter_dimensions" "FFmpeg Filter Builder runner must negotiate graph output dimensions before opening the encoder."
Require-Text $filterRunner "choose_filter_time_base" "FFmpeg Filter Builder runner must preserve sub-frame timestamp precision for setpts speed filters."
Require-Text $filterRunner "video_minimum = {1, 90000}" "FFmpeg Filter Builder runner must use a fine video filter clock for speed-up graphs."
Require-Text $filterSmoke "setpts=PTS/1.5" "FFmpeg Filter Builder smoke test must exercise a real speed-up setpts chain."
Require-Text $filterSmoke "speedReport.duration" "FFmpeg Filter Builder smoke test must inspect the speed-up output rather than only checking exit status."
Require-Text $filterSmoke "crossOriginIsolated" "FFmpeg Filter Builder MT smoke test must verify cross-origin isolation."
Require-Text $filterLauncher "ffmpeg-filter-builder" "FFmpeg Filter Builder must have a Windows launcher."
Require-Text $smokePacker 'line="${line//__THREADING_MODE__/$THREADING_MODE}"' "Smoke packer must replace inline threading placeholders, not only whole-line placeholders."
Require-Text $smokeWindows "Cross-Origin-Opener-Policy" "MT smoke server must send COOP."
Require-Text $smokeWindows "Cross-Origin-Embedder-Policy" "MT smoke server must send COEP."
Require-Text $unixBuild "THREADING_MODE" "Unix build must pass the threading variant into Docker."
Require-Text $windowsBuild "THREADING_MODE" "Windows build must pass the threading variant into Docker."
Require-Text $readme "ffmpeg-filter-builder" "Japanese README must document the FFmpeg Filter Builder profile."
Require-Text $readmeEn "ffmpeg-filter-builder" "English README must document the FFmpeg Filter Builder profile."

$versionsText = [IO.File]::ReadAllText($versions)
if ($versionsText -notmatch '(?m)^BUILDER_VERSION=1\.9\.7$') { throw "Builder version must be 1.9.7." }
foreach ($requiredPin in @(
  'EMSDK_VERSION', 'EMSCRIPTEN_REPOSITORY', 'EMSCRIPTEN_REF', 'EMSCRIPTEN_COMMIT',
  'FFMPEG_REPOSITORY', 'FFMPEG_REF', 'FFMPEG_COMMIT',
  'X264_REPOSITORY', 'X264_FALLBACK_REPOSITORY', 'X264_REF', 'X264_COMMIT',
  'LIBWEBP_REPOSITORY', 'LIBWEBP_FALLBACK_REPOSITORY', 'LIBWEBP_REF', 'LIBWEBP_COMMIT',
  'LIBVPX_REPOSITORY', 'LIBVPX_FALLBACK_REPOSITORY', 'LIBVPX_REF', 'LIBVPX_COMMIT',
  'LIBOPUS_REPOSITORY', 'LIBOPUS_FALLBACK_REPOSITORY', 'LIBOPUS_REF', 'LIBOPUS_COMMIT'
)) {
  $pinPattern = '(?m)^' + [regex]::Escape($requiredPin) + '=.+$'
  if ($versionsText -notmatch $pinPattern) { throw "versions.env is missing $requiredPin." }
}
foreach ($commitName in @('EMSCRIPTEN_COMMIT', 'FFMPEG_COMMIT', 'X264_COMMIT', 'LIBWEBP_COMMIT', 'LIBVPX_COMMIT', 'LIBOPUS_COMMIT')) {
  $commitPattern = '(?m)^' + [regex]::Escape($commitName) + '=([0-9a-f]{40})$'
  $match = [regex]::Match($versionsText, $commitPattern)
  if (-not $match.Success) { throw "$commitName must be a full 40-character lowercase hex commit." }
}

$dockerText = [IO.File]::ReadAllText($dockerfile)
if ($dockerText -match 'cli-builder|export-cli|export-compact|export-all|build-cli') { throw "Dockerfile still contains removed dual-mode stages." }
Require-Text $dockerfile "FROM scratch AS export-no-x264" "Dockerfile must expose a no-x264 export target."
Require-Text $dockerfile "FROM scratch AS export-with-video-codecs" "Dockerfile must expose a combined video-codec export target."
Require-Text $dockerfile "FROM scratch AS export-with-x264" "Dockerfile must expose an x264 export target."
Require-Text $dockerfile "FROM scratch AS export-with-libwebp" "Dockerfile must expose a libwebp export target."
Require-Text $unixBuild 'EXPORT_TARGET="export-no-x264"' "Unix build must skip optional codec libraries when unused."
Require-Text $unixBuild 'EXPORT_TARGET="export-with-video-codecs"' "Unix build must select the combined video codec target."
Require-Text $unixBuild 'EXPORT_TARGET="export-with-x264"' "Unix build must select x264 only when required."
Require-Text $unixBuild 'EXPORT_TARGET="export-with-libwebp"' "Unix build must select libwebp only when required."
Require-Text $windowsBuild '"export-no-x264"' "Windows build must support the no-x264 Docker target."
Require-Text $windowsBuild '"export-with-video-codecs"' "Windows build must support the combined video codec target."
Require-Text $windowsBuild '"export-with-x264"' "Windows build must support the x264 Docker target."
Require-Text $windowsBuild '"export-with-libwebp"' "Windows build must support the libwebp Docker target."

Require-Text $thirdParty 'generated `ffmpeg.wasm`' "Third-party notice must distinguish generated Wasm from the MIT builder source."
Require-Text $thirdParty "lossless-video-cutter" "Third-party notice must explain cutter x264 usage."
Require-Text $thirdParty "media-inspector" "Third-party notice must explain Media Inspector licensing."
Require-Text $thirdParty "video-contact-sheet" "Third-party notice must explain Video Contact Sheet licensing."
Require-Text $thirdParty "GPL-2.0-or-later" "Third-party notice must state video-compressor core licensing."
Require-Text $thirdParty "LGPL-2.1-or-later" "Third-party notice must state cutter core licensing."
Require-Text $licenseIndex "FFmpeg-COPYING.GPLv2" "License index must document FFmpeg GPL license packaging."
Require-Text $licenseIndex "FFmpeg-COPYING.LGPLv2.1" "License index must document FFmpeg LGPL license packaging."
Require-Text $licenseDoc "same GitHub Release" "Licensing docs must keep binaries and corresponding source together."
Require-Text $readme "lossless-video-cutter" "Japanese README must document the cutter profile."
Require-Text $readme "BrowserFFmpeg.losslessVideoCutterArgs" "Japanese README must document the cutter browser helper."
Require-Text $readme "media-inspector" "Japanese README must document the Media Inspector profile."
Require-Text $readme "BrowserFFmpeg.mediaInspectorArgs" "Japanese README must document the Media Inspector browser helper."
Require-Text $readme "video-contact-sheet" "Japanese README must document the Video Contact Sheet profile."
Require-Text $readme "BrowserFFmpeg.videoContactSheetArgs" "Japanese README must document the Video Contact Sheet browser helper."
Require-Text $readme "video-to-gif" "Japanese README must document the GIF profile."
Require-Text $readme "BrowserFFmpeg.videoToGifArgs" "Japanese README must document the GIF browser helper."
Require-Text $readme "video-to-webp" "Japanese README must document the WebP profile."
Require-Text $readme "BrowserFFmpeg.videoToWebpArgs" "Japanese README must document the WebP browser helper."
Require-Text $readmeEn 'does **not** relicense generated `ffmpeg.wasm`' "English README must clearly scope the root MIT license."
Require-Text $releaseDoc "git tag -a v1.9.7" "Release documentation must include the v1.9.7 tag procedure."

Require-Text $releaseScript 'RELEASE_PROFILES=(video-compressor video-speed-changer lossless-video-cutter media-inspector video-contact-sheet video-to-gif video-to-webp ffmpeg-filter-builder)' "Release packer must include all release profiles."
Require-Text $releaseScript 'fetch_exact "FFmpeg"' "Release packer must fetch exact FFmpeg source."
Require-Text $releaseScript 'fetch_exact "x264"' "Release packer must fetch exact x264 source."
Require-Text $releaseScript 'fetch_exact "Emscripten"' "Release packer must fetch exact Emscripten source."
Require-Text $releaseScript 'fetch_exact "libvpx"' "Release packer must fetch exact libvpx source."
Require-Text $releaseScript 'fetch_exact "Opus"' "Release packer must fetch exact Opus source."
Require-Text $releaseScript 'fetch_exact "libwebp"' "Release packer must fetch exact libwebp source."
Require-Text $releaseScript 'PROFILE_USE_LIBVPX' "Release bundle must make libvpx notices profile-specific."
Require-Text $releaseScript 'PROFILE_USE_LIBOPUS' "Release bundle must make Opus notices profile-specific."
Require-Text $releaseScript 'PROFILE_USE_LIBVPX=0' "Release bundle must default missing libvpx profile flags to disabled."
Require-Text $releaseScript 'PROFILE_USE_LIBOPUS=0' "Release bundle must default missing Opus profile flags to disabled."
Require-Text $releaseScript 'PROFILE_USE_LIBWEBP' "Release bundle must make libwebp notices profile-specific."
Require-Text $releaseScript 'PROFILE_USE_X264' "Release bundle must make x264 notices profile-specific."
Require-Text $releaseScript 'PROFILE_BINARY_LICENSE' "Release bundle must choose the FFmpeg license text per profile."
Require-Text $releaseScript 'COPYING.LGPLv2.1' "Release bundle must support LGPL profile licensing."
Require-Text $releaseScript "ffmpeg-wasm-sources-v" "Release packer must create a corresponding-source archive."
Require-Text $releaseScript "sha256sum" "Release packer must generate SHA-256 checksums."

Require-Text $buildWorkflow "lossless-video-cutter" "Main CI must build and smoke-test the cutter."
Require-Text $buildWorkflow "video-compressor" "Main CI must keep testing video compressor."
Require-Text $buildWorkflow "video-speed-changer" "Main CI must build and smoke-test Video Speed Changer."
Require-Text $buildWorkflow "media-inspector" "Main CI must build and smoke-test Media Inspector."
Require-Text $buildWorkflow "video-contact-sheet" "Main CI must build and smoke-test Video Contact Sheet."
Require-Text $buildWorkflow "video-to-gif" "Main CI must build and smoke-test GIF output."
Require-Text $buildWorkflow "video-to-webp" "Main CI must build and smoke-test WebP output."
Require-Text $buildWorkflow "ffmpeg-filter-builder" "Main CI must build and smoke-test both FFmpeg Filter Builder variants."
Require-Text $releaseWorkflow 'tags:' "Release workflow must be tag-driven."
Require-Text $releaseWorkflow 'test "${GITHUB_REF_NAME}" = "v${BUILDER_VERSION}"' "Release workflow must verify tag/version equality."
Require-Text $releaseWorkflow "./build.sh lossless-video-cutter" "Release workflow must smoke-test cutter before publishing."
Require-Text $releaseWorkflow "./build.sh video-compressor" "Release workflow must smoke-test video compressor before publishing."
Require-Text $releaseWorkflow "./build.sh video-speed-changer" "Release workflow must smoke-test Video Speed Changer before publishing."
Require-Text $releaseWorkflow "./build.sh media-inspector" "Release workflow must smoke-test Media Inspector before publishing."
Require-Text $releaseWorkflow "./build.sh video-contact-sheet" "Release workflow must smoke-test Video Contact Sheet before publishing."
Require-Text $releaseWorkflow "./build.sh video-to-gif" "Release workflow must smoke-test GIF before publishing."
Require-Text $releaseWorkflow "./build.sh video-to-webp" "Release workflow must smoke-test WebP before publishing."
Require-Text $releaseWorkflow "./build.sh ffmpeg-filter-builder" "Release workflow must smoke-test both FFmpeg Filter Builder variants before publishing."
Require-Text $releaseWorkflow "ffmpeg-wasm-lossless-video-cutter" "Release workflow must publish the cutter binary bundle."
Require-Text $releaseWorkflow "ffmpeg-wasm-media-inspector" "Release workflow must publish the Media Inspector binary bundle."
Require-Text $releaseWorkflow "ffmpeg-wasm-video-contact-sheet" "Release workflow must publish the Video Contact Sheet binary bundle."
Require-Text $releaseWorkflow "ffmpeg-wasm-video-to-gif" "Release workflow must publish the GIF binary bundle."
Require-Text $releaseWorkflow "ffmpeg-wasm-video-to-webp" "Release workflow must publish the WebP binary bundle."
Require-Text $releaseWorkflow "ffmpeg-wasm-ffmpeg-filter-builder-single-thread" "Release workflow must publish the FFmpeg Filter Builder ST bundle."
Require-Text $releaseWorkflow "ffmpeg-wasm-ffmpeg-filter-builder-multi-thread" "Release workflow must publish the FFmpeg Filter Builder MT bundle."
Require-Text $releaseWorkflow "--verify-tag" "Release creation must refuse an unpushed/missing tag."
Require-Text $releaseWorkflow "contents: write" "Release workflow needs explicit contents:write permission."

$node = Get-Command node -ErrorAction SilentlyContinue
if ($node) {
  & node --check $runtime
  if ($LASTEXITCODE -ne 0) { throw "JavaScript syntax check failed: runtime/browser-ffmpeg.js" }
  foreach ($smokeBody in @($videoSmoke, $speedSmoke, $cutterSmoke, $inspectorSmoke, $contactSmoke, $gifSmoke, $webpSmoke, $filterSmoke)) {
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
