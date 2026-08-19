param(
  [string]$Profile = "video-compressor",
  [int]$TimeoutSeconds = 120
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$Root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$DistRoot = Join-Path $Root "dist"
$ProfileDist = Join-Path $DistRoot $Profile
$HtmlPath = Join-Path $ProfileDist "smoke-test.html"
if (-not (Test-Path $HtmlPath)) {
  throw "Smoke-test HTML was not found: $HtmlPath`nRun build.bat first."
}

function Resolve-Browser {
  $candidates = @()
  if ($env:FFMPEG_WASM_BROWSER) { $candidates += $env:FFMPEG_WASM_BROWSER }

  if ($env:OS -eq "Windows_NT") {
    if ($env:ProgramFiles) {
      $candidates += (Join-Path $env:ProgramFiles "Google\Chrome\Application\chrome.exe")
      $candidates += (Join-Path $env:ProgramFiles "Microsoft\Edge\Application\msedge.exe")
    }
    $pf86 = ${env:ProgramFiles(x86)}
    if ($pf86) {
      $candidates += (Join-Path $pf86 "Google\Chrome\Application\chrome.exe")
      $candidates += (Join-Path $pf86 "Microsoft\Edge\Application\msedge.exe")
    }
    if ($env:LOCALAPPDATA) {
      $candidates += (Join-Path $env:LOCALAPPDATA "Google\Chrome\Application\chrome.exe")
      $candidates += (Join-Path $env:LOCALAPPDATA "Microsoft\Edge\Application\msedge.exe")
    }
  }

  foreach ($name in @("chrome", "google-chrome", "msedge", "chromium", "chromium-browser")) {
    $command = Get-Command $name -ErrorAction SilentlyContinue
    if ($command) { $candidates += $command.Source }
  }
  foreach ($candidate in $candidates) {
    if ($candidate -and (Test-Path $candidate)) { return (Resolve-Path $candidate).Path }
  }
  return $null
}

$Browser = Resolve-Browser
if (-not $Browser) {
  throw @"
A Chromium-based browser was not found for the automatic smoke test.
Microsoft Edge or Google Chrome is enough. You can also set FFMPEG_WASM_BROWSER to the browser executable path.
"@
}

$TempDir = Join-Path ([IO.Path]::GetTempPath()) ("ffmpeg-wasm-smoke-" + [Guid]::NewGuid().ToString("N"))
$BrowserProfile = Join-Path $TempDir "browser-profile"
$StdoutPath = Join-Path $TempDir "stdout.txt"
$StderrPath = Join-Path $TempDir "stderr.txt"
New-Item -ItemType Directory -Force -Path $BrowserProfile | Out-Null
$process = $null

function Read-TextBestEffort {
  param([string]$Path)
  if (-not (Test-Path $Path)) { return "" }
  try {
    $stream = [IO.File]::Open($Path, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::ReadWrite)
    try {
      $reader = New-Object IO.StreamReader($stream)
      try { return $reader.ReadToEnd() } finally { $reader.Dispose() }
    } finally {
      $stream.Dispose()
    }
  } catch {
    return "[Could not read browser stderr yet: $($_.Exception.Message)]"
  }
}

try {
  $Uri = (New-Object System.Uri((Resolve-Path $HtmlPath).Path)).AbsoluteUri
  $BrowserArgs = @(
    "--headless=new",
    "--disable-gpu",
    "--disable-extensions",
    "--no-first-run",
    "--no-default-browser-check",
    "--allow-file-access-from-files",
    "--remote-allow-origins=*",
    "--remote-debugging-port=0",
    "--user-data-dir=$BrowserProfile"
  )
  if ($env:OS -ne "Windows_NT") { $BrowserArgs += "--no-sandbox" }
  $BrowserArgs += $Uri

  Write-Host "[FFmpeg WASM] Smoke test: actual browser transcode" -ForegroundColor Cyan
  Write-Host "[FFmpeg WASM] Browser: $Browser" -ForegroundColor DarkGray
  Write-Host "[FFmpeg WASM] Input:   tests/fixtures/smoke-input.mp4" -ForegroundColor DarkGray

  $process = Start-Process -FilePath $Browser -ArgumentList $BrowserArgs -PassThru `
    -RedirectStandardOutput $StdoutPath -RedirectStandardError $StderrPath

  $portFile = Join-Path $BrowserProfile "DevToolsActivePort"
  $startupDeadline = [DateTime]::UtcNow.AddSeconds(15)
  while (-not (Test-Path $portFile) -and [DateTime]::UtcNow -lt $startupDeadline) {
    if ($process.HasExited) { break }
    Start-Sleep -Milliseconds 100
  }
  if (-not (Test-Path $portFile)) {
    $stderr = Read-TextBestEffort $StderrPath
    throw "Headless browser did not expose a DevTools port.`n$stderr"
  }

  $port = (Get-Content $portFile -TotalCount 1).Trim()
  if ($port -notmatch '^\d+$') { throw "Invalid DevTools port: $port" }
  $endpoint = "http://127.0.0.1:$port/json/list"
  $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
  $lastUrl = ""

  while ([DateTime]::UtcNow -lt $deadline) {
    if ($process.HasExited) { throw "Headless browser exited before the smoke test completed." }
    try {
      $targets = Invoke-RestMethod -Uri $endpoint -TimeoutSec 3
      foreach ($target in @($targets)) {
        if ($target.type -ne "page") { continue }
        $targetUrl = [string]$target.url
        $lastUrl = $targetUrl
        if ($targetUrl -match '#SMOKE_TEST_PASS_(.*)$') {
          $details = [Uri]::UnescapeDataString($Matches[1])
          Write-Host ("[OK] Smoke test passed. {0}" -f $details) -ForegroundColor Green
          return
        }
        if ($targetUrl -match '#SMOKE_TEST_FAIL_(.*)$') {
          $message = [Uri]::UnescapeDataString($Matches[1])
          throw "SMOKE_TEST_FAIL: $message"
        }
      }
    } catch {
      if ($_.Exception.Message.StartsWith("SMOKE_TEST_FAIL:")) { throw }
    }
    Start-Sleep -Milliseconds 250
  }

  $stderr = Read-TextBestEffort $StderrPath
  $tail = if ($stderr.Length -gt 4000) { $stderr.Substring($stderr.Length - 4000) } else { $stderr }
  throw "Smoke test timed out after $TimeoutSeconds seconds. Last page URL: $lastUrl`nBrowser stderr:`n$tail"
} finally {
  if ($process -and -not $process.HasExited) {
    try { $process.Kill() } catch {}
    try { $process.WaitForExit(5000) | Out-Null } catch {}
  }
  if (Test-Path $TempDir) { Remove-Item -Recurse -Force $TempDir -ErrorAction SilentlyContinue }
}
