param(
  [Parameter(Mandatory=$true, Position=0)]
  [ValidateSet('Start', 'Collect')][string]$Action,
  [string]$Adb = 'adb',
  [string]$Device,
  [switch]$Parallel,
  [switch]$NoValidation
)
$ErrorActionPreference = 'Stop'
$app = 'org.opengothic.gothic2notr.diagnostics'
$root = "/sdcard/Android/data/$app/files"
$selection = @()
if($Device) { $selection = @('-s', $Device) }
function Invoke-Adb {
  $result = & $Adb @selection @args
  if($LASTEXITCODE -ne 0) { throw "ADB command failed: $args" }
  $result
}
$attemptFile = Join-Path $PSScriptRoot 'adreno-attempt.json'
if($Action -eq 'Start') {
  Invoke-Adb shell am force-stop $app
  Invoke-Adb shell mkdir -p $root
  foreach($setting in @(@('pipeline-parallel', [bool]$Parallel), @('pipeline-no-validation', [bool]$NoValidation))) {
    if($setting[1]) { Invoke-Adb shell touch "$root/$($setting[0])" }
    else { Invoke-Adb shell rm -f "$root/$($setting[0])" }
  }
  $epoch = (Invoke-Adb shell date '+%s').Trim()
  $since = (Invoke-Adb shell 'date "+%m-%d %H:%M:%S.000"').Trim()
  @{since=$since; epoch=$epoch; parallel=[bool]$Parallel; validation=![bool]$NoValidation; device=$Device} |
    ConvertTo-Json | Set-Content -LiteralPath $attemptFile
  Invoke-Adb shell am start -n "$app/org.opengothic.app.SetupActivity"
  Write-Host 'Reproduce the crash, then run this helper with Collect without reopening the app.'
  exit
}
if(-not (Test-Path -LiteralPath $attemptFile)) { throw 'Run Start before Collect.' }
$attempt = Get-Content -Raw -LiteralPath $attemptFile | ConvertFrom-Json
if($attempt.device -ne $Device) { throw 'Use the same -Device value as Start.' }
$results = Join-Path $PSScriptRoot ('adreno-results-'+(Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Path $results | Out-Null
Copy-Item -LiteralPath $attemptFile -Destination (Join-Path $results 'attempt.json')
Invoke-Adb logcat -b all -d -v threadtime -T $attempt.since 'app:V' 'DEBUG:F' 'libc:F' 'VALIDATION:V' '*:S' |
  Set-Content -LiteralPath (Join-Path $results 'logcat.txt')
Invoke-Adb pull "$root/pipeline-captures" $results
Invoke-Adb pull "$root/log.txt" $results
Invoke-Adb shell dumpsys package $app | Set-Content -LiteralPath (Join-Path $results 'package.txt')
Compress-Archive -LiteralPath $results -DestinationPath "$results.zip"
Write-Host "Send $results.zip. Game data and saves were not collected."
