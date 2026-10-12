param(
    [Parameter(Mandatory = $true)]
    [string] $Path
)

$ErrorActionPreference = 'Stop'
$culture = [System.Globalization.CultureInfo]::InvariantCulture
$frames = @{}
foreach ($row in Import-Csv -LiteralPath $Path) {
    $frame = [uint32]::Parse($row.frame, $culture)
    $duration = [double]::Parse($row.milliseconds, $culture)
    if ([double]::IsNaN($duration) -or [double]::IsInfinity($duration) -or $duration -lt 0) {
        throw "Invalid GPU duration in frame $frame."
    }
    if (-not $frames.ContainsKey($frame)) {
        $frames[$frame] = @{}
    }
    # Repeated markers, such as visibility passes, contribute to the same frame total.
    $frames[$frame][$row.pass] += $duration
}
if ($frames.Count -eq 0) {
    throw 'No completed GPU frames in the CSV. Check profiling support and load a game first.'
}

$passes = @{}
$totals = @()
foreach ($frame in $frames.Values) {
    $total = 0.0
    foreach ($name in $frame.Keys) {
        $value = $frame[$name]
        $total += $value
        if (-not $passes.ContainsKey($name)) {
            $passes[$name] = @{ Sum = 0.0; Max = 0.0; Frames = 0 }
        }
        $passes[$name].Sum += $value
        $passes[$name].Max = [math]::Max($passes[$name].Max, $value)
        $passes[$name].Frames++
    }
    $totals += $total
}

Write-Output "Completed frames: $($frames.Count)"
Write-Output ('Mean GPU marker span: {0:F3} ms' -f (($totals | Measure-Object -Average).Average))
Write-Output 'Elapsed marker intervals include stalls and pipeline overlap; they are not isolated shader costs.'
$summary = foreach ($name in $passes.Keys) {
    [pscustomobject]@{
        Pass = $name
        MeanMsPerFrame = [math]::Round($passes[$name].Sum / $frames.Count, 3)
        MaxMsInFrame = [math]::Round($passes[$name].Max, 3)
        FramesPresent = $passes[$name].Frames
    }
}
$summary | Sort-Object MeanMsPerFrame -Descending | Format-Table -AutoSize
