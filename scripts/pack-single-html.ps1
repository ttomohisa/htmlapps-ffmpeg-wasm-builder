param(
  [string]$Profile = "video-compressor",
  [string]$Output = ""
)
$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$Root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$DistDir = Join-Path $Root ("dist\" + $Profile)
$TemplatePath = Join-Path $Root ("profiles\" + $Profile + "\single-html\template.html")
$RuntimePath = Join-Path $Root "runtime\browser-ffmpeg.js"

if ([string]::IsNullOrWhiteSpace($Output)) {
  $Output = Join-Path $Root ("dist\single-html-" + $Profile + ".html")
} elseif (-not [System.IO.Path]::IsPathRooted($Output)) {
  $Output = Join-Path $Root $Output
}

$required = @(
  $TemplatePath,
  $RuntimePath,
  (Join-Path $DistDir "ffmpeg.js.gz"),
  (Join-Path $DistDir "ffmpeg.wasm.gz")
)
foreach ($path in $required) {
  if (-not (Test-Path $path)) { throw "Required file was not found: $path" }
}

$template = [IO.File]::ReadAllText($TemplatePath, [Text.Encoding]::UTF8)
$runtime = [IO.File]::ReadAllText($RuntimePath, [Text.Encoding]::UTF8)
$template = $template.Replace("__FFMPEG_RUNTIME__", $runtime)
$template = $template.Replace("__FFMPEG_JS_GZIP_BASE64__", [Convert]::ToBase64String([IO.File]::ReadAllBytes((Join-Path $DistDir "ffmpeg.js.gz"))))
$template = $template.Replace("__FFMPEG_WASM_GZIP_BASE64__", [Convert]::ToBase64String([IO.File]::ReadAllBytes((Join-Path $DistDir "ffmpeg.wasm.gz"))))

if ($template -match '__(?:FFMPEG_(?:JS_GZIP_BASE64|WASM_GZIP_BASE64|RUNTIME))__') {
  throw "A packaging placeholder remains in the output: $($Matches[0])"
}

New-Item -ItemType Directory -Force -Path (Split-Path -Parent $Output) | Out-Null
[IO.File]::WriteAllText($Output, $template, (New-Object Text.UTF8Encoding($false)))
Write-Host "[OK] Single HTML: $Output" -ForegroundColor Green
Write-Host ("[OK] Size: {0:N2} MB" -f ((Get-Item $Output).Length / 1MB))
