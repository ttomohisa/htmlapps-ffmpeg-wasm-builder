param()

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$Root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$VersionsPath = Join-Path $Root "versions.env"

$values = @{}
Get-Content -Encoding UTF8 $VersionsPath | ForEach-Object {
  $line = $_.Trim()
  if (-not $line -or $line.StartsWith("#")) { return }
  $parts = $line.Split("=", 2)
  if ($parts.Count -eq 2) { $values[$parts[0].Trim()] = $parts[1].Trim() }
}

Write-Host "Current pins" -ForegroundColor Cyan
Write-Host "  Builder:     $($values['BUILDER_VERSION'])"
Write-Host "  FFmpeg:      $($values['FFMPEG_REF']) ($($values['FFMPEG_COMMIT']))"
Write-Host "  Emscripten:  $($values['EMSDK_VERSION']) ($($values['EMSCRIPTEN_COMMIT']))"
Write-Host "  x264:        $($values['X264_COMMIT'])"
Write-Host ""

Write-Host "Checking FFmpeg..." -ForegroundColor Cyan
$downloadPage = (Invoke-WebRequest -UseBasicParsing "https://ffmpeg.org/download.html").Content
$versions = [regex]::Matches($downloadPage, 'ffmpeg-(\d+\.\d+(?:\.\d+)?)\.tar\.xz') |
  ForEach-Object { $_.Groups[1].Value } |
  Sort-Object { [version]$_ } -Descending -Unique
$latestFfmpeg = $versions | Select-Object -First 1
if (-not $latestFfmpeg) { throw "Could not determine latest FFmpeg version." }

$ref = Invoke-RestMethod -Headers @{ "User-Agent" = "htmlapps-ffmpeg-wasm-builder" } `
  "https://api.github.com/repos/FFmpeg/FFmpeg/git/ref/tags/n$latestFfmpeg"
$ffmpegCommit = $ref.object.sha
if ($ref.object.type -eq "tag") {
  $tag = Invoke-RestMethod -Headers @{ "User-Agent" = "htmlapps-ffmpeg-wasm-builder" } $ref.object.url
  $ffmpegCommit = $tag.object.sha
}

Write-Host "Checking Emscripten..." -ForegroundColor Cyan
$emsdk = Invoke-RestMethod `
  "https://raw.githubusercontent.com/emscripten-core/emsdk/main/emscripten-releases-tags.json"
$latestEmsdk = $emsdk.aliases.latest
$emsRef = Invoke-RestMethod -Headers @{ "User-Agent" = "htmlapps-ffmpeg-wasm-builder" } `
  "https://api.github.com/repos/emscripten-core/emscripten/git/ref/tags/$latestEmsdk"
$emscriptenCommit = $emsRef.object.sha
if ($emsRef.object.type -eq "tag") {
  $emsTag = Invoke-RestMethod -Headers @{ "User-Agent" = "htmlapps-ffmpeg-wasm-builder" } $emsRef.object.url
  $emscriptenCommit = $emsTag.object.sha
}

Write-Host "Checking x264 stable..." -ForegroundColor Cyan
$latestX264 = ""
if (Get-Command git -ErrorAction SilentlyContinue) {
  $line = (& git ls-remote $values['X264_REPOSITORY'] "refs/heads/$($values['X264_REF'])" 2>$null | Select-Object -First 1)
  if (-not $line -and $values['X264_FALLBACK_REPOSITORY']) {
    $line = (& git ls-remote $values['X264_FALLBACK_REPOSITORY'] "refs/heads/$($values['X264_REF'])" 2>$null | Select-Object -First 1)
  }
  if ($line) { $latestX264 = ($line -split "\s+")[0] }
}

Write-Host ""
Write-Host "Latest detected" -ForegroundColor Green
Write-Host "  FFmpeg:      n$latestFfmpeg"
Write-Host "  commit:      $ffmpegCommit"
Write-Host "  Emscripten:  $latestEmsdk"
Write-Host "  commit:      $emscriptenCommit"
if ($latestX264) {
  Write-Host "  x264:        $latestX264"
} else {
  Write-Host "  x264:        could not check (Git is required)"
}

Write-Host ""
Write-Host "Suggested versions.env values" -ForegroundColor Yellow
Write-Host "EMSDK_VERSION=$latestEmsdk"
Write-Host "EMSCRIPTEN_REF=$latestEmsdk"
Write-Host "EMSCRIPTEN_COMMIT=$emscriptenCommit"
Write-Host "FFMPEG_REF=n$latestFfmpeg"
Write-Host "FFMPEG_COMMIT=$ffmpegCommit"
if ($latestX264) {
  Write-Host "X264_COMMIT=$latestX264"
}

Write-Host ""
Write-Host "Do not update production blindly. Change one pin at a time, then run build.bat and require the browser smoke test to pass." -ForegroundColor DarkYellow
