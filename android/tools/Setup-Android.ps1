$ErrorActionPreference = 'Stop'

function Find-Python {
    $candidates = @('python.exe', 'py.exe')
    $candidates += @(Get-ChildItem "$env:LOCALAPPDATA/Programs/Python/Python*/python.exe" -ErrorAction SilentlyContinue | ForEach-Object FullName)
    foreach ($candidate in $candidates) {
        $command = Get-Command $candidate -ErrorAction SilentlyContinue
        if (!$command -or $command.Source -like '*WindowsApps*') { continue }
        & $command.Source -c 'import sys; sys.exit(sys.version_info < (3, 10))' 2>$null
        if ($LASTEXITCODE -eq 0) { return $command.Source }
    }
    return $null
}

try {
    $python = Find-Python
    if (!$python) {
        $answer = Read-Host 'Install Python 3.13 for your account using Windows Package Manager? [Y/n]'
        if ($answer -and $answer -notmatch '^(?i:y|yes)$') { throw 'Python 3.10 or newer is required. Nothing was installed.' }
        if (!(Get-Command winget.exe -ErrorAction SilentlyContinue)) {
            throw 'Windows Package Manager is missing. Install App Installer from Microsoft Store, or Python from https://www.python.org/downloads/windows/, then rerun. No administrator account is needed for a per-user Python install.'
        }
        # Winget verifies its package checksum and presents any required agreements.
        & winget.exe install --exact --id Python.Python.3.13 --source winget --scope user
        if ($LASTEXITCODE -ne 0) { throw 'Python installation did not finish.' }
        $python = Find-Python
        if (!$python) { throw 'Open a new terminal and rerun so the installed Python can be found.' }
    }
    & $python "$PSScriptRoot/setup_android.py" @args
    exit $LASTEXITCODE
} catch {
    Write-Host $_.Exception.Message -ForegroundColor Red
    exit 1
}
