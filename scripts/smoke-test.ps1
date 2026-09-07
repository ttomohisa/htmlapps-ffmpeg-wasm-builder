param(
  [string]$Profile = "video-compressor",
  [ValidateSet("single-thread", "multi-thread")][string]$Threading = "single-thread",
  [int]$TimeoutSeconds = 120
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$Root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$DistRoot = Join-Path $Root "dist"
$ProfileRoot = Join-Path $DistRoot $Profile
$VariantCandidate = Join-Path $ProfileRoot $Threading
$ProfileDist = if (Test-Path $VariantCandidate -PathType Container) { $VariantCandidate } else { $ProfileRoot }
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
$serverJob = $null

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

function Get-FreeTcpPort {
  $listener = New-Object System.Net.Sockets.TcpListener([System.Net.IPAddress]::Loopback, 0)
  $listener.Start()
  try { return ([System.Net.IPEndPoint]$listener.LocalEndpoint).Port } finally { $listener.Stop() }
}

function Start-CrossOriginIsolatedServer {
  param([string]$RootPath, [int]$Port)
  return Start-Job -ArgumentList $RootPath, $Port -ScriptBlock {
    param($ServeRoot, $ServePort)
    $listener = New-Object System.Net.HttpListener
    $listener.Prefixes.Add("http://127.0.0.1:$ServePort/")
    $listener.Start()
    try {
      while ($listener.IsListening) {
        $context = $listener.GetContext()
        try {
          $relative = [Uri]::UnescapeDataString($context.Request.Url.AbsolutePath.TrimStart('/'))
          if ([string]::IsNullOrWhiteSpace($relative)) { $relative = "smoke-test.html" }
          $full = [IO.Path]::GetFullPath((Join-Path $ServeRoot $relative))
          $rootFull = [IO.Path]::GetFullPath($ServeRoot).TrimEnd([IO.Path]::DirectorySeparatorChar) + [IO.Path]::DirectorySeparatorChar
          if (-not $full.StartsWith($rootFull, [StringComparison]::OrdinalIgnoreCase) -or -not (Test-Path $full -PathType Leaf)) {
            $context.Response.StatusCode = 404
            $context.Response.Close()
            continue
          }
          $bytes = [IO.File]::ReadAllBytes($full)
          $context.Response.Headers.Add("Cross-Origin-Opener-Policy", "same-origin")
          $context.Response.Headers.Add("Cross-Origin-Embedder-Policy", "require-corp")
          $context.Response.Headers.Add("Cross-Origin-Resource-Policy", "same-origin")
          $context.Response.ContentType = if ($full.EndsWith(".html")) { "text/html; charset=utf-8" } elseif ($full.EndsWith(".js")) { "text/javascript; charset=utf-8" } else { "application/octet-stream" }
          $context.Response.ContentLength64 = $bytes.Length
          if ($context.Request.HttpMethod -ne "HEAD") {
            $context.Response.OutputStream.Write($bytes, 0, $bytes.Length)
          }
          $context.Response.Close()
        } catch {
          try { $context.Response.StatusCode = 500; $context.Response.Close() } catch {}
        }
      }
    } finally {
      if ($listener.IsListening) { $listener.Stop() }
      $listener.Close()
    }
  }
}

try {
  if ($Threading -eq "multi-thread") {
    $ServerPort = Get-FreeTcpPort
    $serverJob = Start-CrossOriginIsolatedServer -RootPath $ProfileDist -Port $ServerPort
    $Uri = "http://127.0.0.1:$ServerPort/smoke-test.html"
    $serverDeadline = [DateTime]::UtcNow.AddSeconds(10)
    $serverReady = $false
    do {
      try {
        $probe = Invoke-WebRequest -Uri $Uri -Method Head -TimeoutSec 1 -UseBasicParsing
        if ($probe.StatusCode -eq 200) { $serverReady = $true; break }
      } catch {}
      Start-Sleep -Milliseconds 100
    } while ([DateTime]::UtcNow -lt $serverDeadline)
    if (-not $serverReady) {
      $jobState = if ($serverJob) { [string]$serverJob.State } else { "missing" }
      throw "Could not start the cross-origin-isolated smoke server. Job state: $jobState"
    }
  } else {
    $Uri = (New-Object System.Uri((Resolve-Path $HtmlPath).Path)).AbsoluteUri
  }
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

  $smokeKind = if ($Threading -eq "multi-thread") { "actual browser transcode (COOP/COEP + pthread)" } else { "actual browser transcode (file:// single-thread)" }
  Write-Host "[FFmpeg WASM] Smoke test: $smokeKind" -ForegroundColor Cyan
  Write-Host "[FFmpeg WASM] Threading: $Threading" -ForegroundColor DarkGray
  Write-Host "[FFmpeg WASM] Browser: $Browser" -ForegroundColor DarkGray
  $fixtureName = if ($Profile -eq "video-compressor") { "tests/fixtures/smoke-rotated.mp4" } else { "tests/fixtures/smoke-input.mp4" }
  Write-Host "[FFmpeg WASM] Input:   $fixtureName" -ForegroundColor DarkGray

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
  if ($serverJob) {
    try { Stop-Job $serverJob -ErrorAction SilentlyContinue | Out-Null } catch {}
    try { Remove-Job $serverJob -Force -ErrorAction SilentlyContinue | Out-Null } catch {}
  }
  if (Test-Path $TempDir) { Remove-Item -Recurse -Force $TempDir -ErrorAction SilentlyContinue }
}
