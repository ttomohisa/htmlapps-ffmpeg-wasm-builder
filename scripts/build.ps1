param(
  [string]$Profile = "video-compressor",
  [switch]$SkipSmokeTest
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$Root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$VersionsPath = Join-Path $Root "versions.env"
$Dockerfile = Join-Path $Root "docker\Dockerfile"
$ProfileRoot = Join-Path $Root ("dist\" + $Profile)

if (-not (Get-Command docker -ErrorAction SilentlyContinue)) {
  throw "Docker was not found. Install/start Docker Desktop, then run build.bat again."
}

function Test-NativeCommandQuiet {
  param(
    [Parameter(Mandatory = $true)][string]$Command,
    [string[]]$Arguments = @()
  )
  $previousPreference = $ErrorActionPreference
  $exitCode = 1
  try {
    $ErrorActionPreference = "Continue"
    & $Command @Arguments *> $null
    $exitCode = $LASTEXITCODE
  } finally {
    $ErrorActionPreference = $previousPreference
  }
  return ($exitCode -eq 0)
}

if (-not (Test-NativeCommandQuiet "docker" @("version", "--format", "{{.Server.Version}}"))) {
  throw "Docker Desktop is installed, but its Linux engine is not running."
}
if (-not (Test-NativeCommandQuiet "docker" @("buildx", "version"))) {
  throw "Docker Buildx is not available. Update Docker Desktop."
}

$values = @{}
Get-Content -Encoding UTF8 $VersionsPath | ForEach-Object {
  $line = $_.Trim()
  if (-not $line -or $line.StartsWith("#")) { return }
  $parts = $line.Split("=", 2)
  if ($parts.Count -ne 2) { throw "Invalid versions.env line: $line" }
  $values[$parts[0].Trim()] = $parts[1].Trim()
}

$required = @(
  "BUILDER_VERSION", "EMSDK_VERSION", "EMSCRIPTEN_REPOSITORY", "EMSCRIPTEN_REF", "EMSCRIPTEN_COMMIT",
  "FFMPEG_REPOSITORY", "FFMPEG_REF", "FFMPEG_COMMIT",
  "X264_REPOSITORY", "X264_FALLBACK_REPOSITORY", "X264_REF", "X264_COMMIT",
  "LIBWEBP_REPOSITORY", "LIBWEBP_FALLBACK_REPOSITORY", "LIBWEBP_REF", "LIBWEBP_COMMIT",
  "LIBVPX_REPOSITORY", "LIBVPX_FALLBACK_REPOSITORY", "LIBVPX_REF", "LIBVPX_COMMIT",
  "LIBOPUS_REPOSITORY", "LIBOPUS_FALLBACK_REPOSITORY", "LIBOPUS_REF", "LIBOPUS_COMMIT"
)
foreach ($name in $required) {
  if (-not $values.ContainsKey($name) -or [string]::IsNullOrWhiteSpace($values[$name])) {
    throw "versions.env is missing: $name"
  }
}

$profileFile = Join-Path $Root ("profiles\" + $Profile + "\ffmpeg.flags")
$profileConfig = Join-Path $Root ("profiles\" + $Profile + "\profile.env")
$runnerFile = Join-Path $Root ("runners\" + $Profile + ".c")
if (-not (Test-Path $profileFile)) { throw "Build profile is missing: $profileFile" }
if (-not (Test-Path $profileConfig)) { throw "Build profile metadata is missing: $profileConfig" }
if (-not (Test-Path $runnerFile)) { throw "Runner is missing: $runnerFile" }

$profileConfigText = [IO.File]::ReadAllText($profileConfig)
function Read-BoolSetting([string]$Name, [bool]$Required = $false) {
  $match = [regex]::Match($profileConfigText, ('(?m)^' + [regex]::Escape($Name) + '=(0|1)\s*$'))
  if (-not $match.Success) {
    if ($Required) { throw "profile.env must contain $Name=0 or 1: $profileConfig" }
    return $false
  }
  return $match.Groups[1].Value -eq "1"
}
$UseX264 = Read-BoolSetting "PROFILE_USE_X264" $true
$UseLibwebp = Read-BoolSetting "PROFILE_USE_LIBWEBP" $true
$UseLibvpx = Read-BoolSetting "PROFILE_USE_LIBVPX"
$UseLibopus = Read-BoolSetting "PROFILE_USE_LIBOPUS"
if ($UseX264 -and $UseLibwebp) { throw "Profiles cannot currently link x264 and libwebp together." }

$threadingMatch = [regex]::Match($profileConfigText, '(?m)^PROFILE_THREADING_VARIANTS="([^"]+)"\s*$')
$threadingVariants = if ($threadingMatch.Success) { @($threadingMatch.Groups[1].Value.Split(',') | ForEach-Object { $_.Trim() } | Where-Object { $_ }) } else { @("single-thread") }
foreach ($variant in $threadingVariants) {
  if ($variant -ne "single-thread" -and $variant -ne "multi-thread") {
    throw "Unsupported PROFILE_THREADING_VARIANTS entry: $variant"
  }
}
$threadingVariants = @($threadingVariants | Select-Object -Unique)
$isDual = $threadingVariants.Count -gt 1

$ExportTarget = if ($UseX264 -and $UseLibvpx -and $UseLibopus -and -not $UseLibwebp) {
  "export-with-video-codecs"
} elseif ($UseX264) {
  "export-with-x264"
} elseif ($UseLibwebp) {
  "export-with-libwebp"
} else {
  "export-no-x264"
}

if (Test-Path $ProfileRoot) { Remove-Item -Recurse -Force $ProfileRoot }
New-Item -ItemType Directory -Force -Path $ProfileRoot | Out-Null

foreach ($Threading in $threadingVariants) {
  $OutDir = if ($isDual) { Join-Path $ProfileRoot $Threading } else { $ProfileRoot }
  New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

  $DockerArgs = @(
    "buildx", "build",
    "--file", $Dockerfile,
    "--target", $ExportTarget,
    "--build-arg", "BUILDER_VERSION=$($values['BUILDER_VERSION'])",
    "--build-arg", "EMSDK_VERSION=$($values['EMSDK_VERSION'])",
    "--build-arg", "EMSCRIPTEN_COMMIT=$($values['EMSCRIPTEN_COMMIT'])",
    "--build-arg", "FFMPEG_REPOSITORY=$($values['FFMPEG_REPOSITORY'])",
    "--build-arg", "FFMPEG_REF=$($values['FFMPEG_REF'])",
    "--build-arg", "FFMPEG_COMMIT=$($values['FFMPEG_COMMIT'])",
    "--build-arg", "X264_REPOSITORY=$($values['X264_REPOSITORY'])",
    "--build-arg", "X264_FALLBACK_REPOSITORY=$($values['X264_FALLBACK_REPOSITORY'])",
    "--build-arg", "X264_REF=$($values['X264_REF'])",
    "--build-arg", "X264_COMMIT=$($values['X264_COMMIT'])",
    "--build-arg", "LIBWEBP_REPOSITORY=$($values['LIBWEBP_REPOSITORY'])",
    "--build-arg", "LIBWEBP_FALLBACK_REPOSITORY=$($values['LIBWEBP_FALLBACK_REPOSITORY'])",
    "--build-arg", "LIBWEBP_REF=$($values['LIBWEBP_REF'])",
    "--build-arg", "LIBWEBP_COMMIT=$($values['LIBWEBP_COMMIT'])",
    "--build-arg", "LIBVPX_REPOSITORY=$($values['LIBVPX_REPOSITORY'])",
    "--build-arg", "LIBVPX_FALLBACK_REPOSITORY=$($values['LIBVPX_FALLBACK_REPOSITORY'])",
    "--build-arg", "LIBVPX_REF=$($values['LIBVPX_REF'])",
    "--build-arg", "LIBVPX_COMMIT=$($values['LIBVPX_COMMIT'])",
    "--build-arg", "LIBOPUS_REPOSITORY=$($values['LIBOPUS_REPOSITORY'])",
    "--build-arg", "LIBOPUS_FALLBACK_REPOSITORY=$($values['LIBOPUS_FALLBACK_REPOSITORY'])",
    "--build-arg", "LIBOPUS_REF=$($values['LIBOPUS_REF'])",
    "--build-arg", "LIBOPUS_COMMIT=$($values['LIBOPUS_COMMIT'])",
    "--build-arg", "PROFILE=$Profile",
    "--build-arg", "THREADING_MODE=$Threading",
    "--output", "type=local,dest=$OutDir",
    $Root
  )

  Write-Host "[FFmpeg WASM] Profile: $Profile" -ForegroundColor Cyan
  if ($Threading -eq "multi-thread") {
    Write-Host "[FFmpeg WASM] Architecture: public-libav runner / pthread / SharedArrayBuffer" -ForegroundColor Cyan
  } else {
    Write-Host "[FFmpeg WASM] Architecture: public-libav runner / single-thread / no SharedArrayBuffer" -ForegroundColor Cyan
  }
  Write-Host "[FFmpeg WASM] Variant: $Threading" -ForegroundColor Cyan
  Write-Host "[FFmpeg WASM] Docker target: $ExportTarget" -ForegroundColor DarkGray
  Write-Host "[FFmpeg WASM] First build downloads a large Emscripten image; later builds reuse Docker cache." -ForegroundColor DarkGray
  Write-Host "[FFmpeg WASM] Builder: selected Buildx builder (no forced builder/context)" -ForegroundColor DarkGray

  & docker @DockerArgs
  if ($LASTEXITCODE -ne 0) { throw "Docker build failed with exit code $LASTEXITCODE ($Threading)" }

  $requiredOutputs = @("ffmpeg.js", "ffmpeg.wasm", "ffmpeg.js.gz", "ffmpeg.wasm.gz", "manifest.json", "smoke-test.html")
  foreach ($relative in $requiredOutputs) {
    $path = Join-Path $OutDir $relative
    if (-not (Test-Path $path) -or (Get-Item $path).Length -eq 0) { throw "Build output is missing: $path" }
  }
  if ((Test-Path (Join-Path $OutDir "ffmpeg.worker.js")) -or (Test-Path (Join-Path $OutDir "ffmpeg.worker.js.gz"))) {
    throw "Unexpected legacy pthread worker asset in $OutDir; Emscripten 6.x reuses ffmpeg.js via mainScriptUrlOrBlob"
  }

  Write-Host ""
  Write-Host "[OK] Build completed: $OutDir" -ForegroundColor Green
  Get-ChildItem $OutDir -File |
    Sort-Object Name |
    Select-Object Name, @{N="MB";E={[Math]::Round($_.Length / 1MB, 2)}} |
    Format-Table -AutoSize

  if (-not $SkipSmokeTest) {
    & (Join-Path $Root "scripts\smoke-test.ps1") -Profile $Profile -Threading $Threading
    if (-not $?) { throw "Smoke test failed: $Profile / $Threading" }
  } else {
    Write-Host "[WARN] Smoke test skipped by request: $Threading" -ForegroundColor Yellow
  }
}

if ($isDual) {
  Write-Host ""
  Write-Host "[OK] Dual-runtime build completed: $ProfileRoot" -ForegroundColor Green
  Write-Host "     single-thread/ = portable/file:// candidate" -ForegroundColor DarkGray
  Write-Host "     multi-thread/  = cross-origin-isolated hosted candidate" -ForegroundColor DarkGray
}
