param(
  [string]$Profile = "video-compressor",
  [switch]$SkipSmokeTest
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$Root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$VersionsPath = Join-Path $Root "versions.env"
$Dockerfile = Join-Path $Root "docker\Dockerfile"
$OutDir = Join-Path $Root ("dist\" + $Profile)

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
  "X264_REPOSITORY", "X264_FALLBACK_REPOSITORY", "X264_REF", "X264_COMMIT"
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
$useX264Match = [regex]::Match($profileConfigText, '(?m)^PROFILE_USE_X264=(0|1)\s*$')
if (-not $useX264Match.Success) { throw "profile.env must contain PROFILE_USE_X264=0 or 1: $profileConfig" }
$ExportTarget = if ($useX264Match.Groups[1].Value -eq "1") { "export-with-x264" } else { "export-no-x264" }

if (Test-Path $OutDir) { Remove-Item -Recurse -Force $OutDir }
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
  "--build-arg", "PROFILE=$Profile",
  "--output", "type=local,dest=$OutDir",
  $Root
)

Write-Host "[FFmpeg WASM] Profile: $Profile" -ForegroundColor Cyan
Write-Host "[FFmpeg WASM] Architecture: public-libav runner / single-thread / no SharedArrayBuffer" -ForegroundColor Cyan
Write-Host "[FFmpeg WASM] Docker target: $ExportTarget" -ForegroundColor DarkGray
Write-Host "[FFmpeg WASM] First build downloads a large Emscripten image; later builds reuse Docker cache." -ForegroundColor DarkGray
Write-Host "[FFmpeg WASM] Builder: selected Buildx builder (no forced builder/context)" -ForegroundColor DarkGray

& docker @DockerArgs
if ($LASTEXITCODE -ne 0) { throw "Docker build failed with exit code $LASTEXITCODE" }

foreach ($relative in @("ffmpeg.js", "ffmpeg.wasm", "ffmpeg.js.gz", "ffmpeg.wasm.gz", "manifest.json", "smoke-test.html")) {
  $path = Join-Path $OutDir $relative
  if (-not (Test-Path $path) -or (Get-Item $path).Length -eq 0) { throw "Build output is missing: $path" }
}

Write-Host ""
Write-Host "[OK] Build completed: $OutDir" -ForegroundColor Green
Get-ChildItem $OutDir -File |
  Sort-Object Name |
  Select-Object Name, @{N="MB";E={[Math]::Round($_.Length / 1MB, 2)}} |
  Format-Table -AutoSize

if (-not $SkipSmokeTest) {
  & (Join-Path $Root "scripts\smoke-test.ps1") -Profile $Profile
  if (-not $?) { throw "Smoke test failed." }
} else {
  Write-Host "[WARN] Smoke test skipped by request." -ForegroundColor Yellow
}
