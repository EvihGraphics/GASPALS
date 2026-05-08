Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$script:ReferenceAllCases = @('10', '11', '20', '30', '31', '40')

function Get-ReferenceRoot {
    param(
        [string]$StartDir = $PSScriptRoot
    )

    return (Resolve-Path (Join-Path $StartDir '..\..')).Path
}

function Ensure-ReferenceDirectory {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    if (-not (Test-Path $Path)) {
        New-Item -ItemType Directory -Path $Path -Force | Out-Null
    }
    return (Resolve-Path $Path).Path
}

function Resolve-ReferenceCases {
    param(
        [string[]]$Cases
    )

    if ($null -eq $Cases -or $Cases.Count -eq 0) {
        return $script:ReferenceAllCases
    }

    $expanded = New-Object System.Collections.Generic.List[string]
    foreach ($entry in $Cases) {
        foreach ($token in ($entry -split '[,\s]+' | Where-Object { -not [string]::IsNullOrWhiteSpace($_) })) {
            if ($token -ieq 'all') {
                return $script:ReferenceAllCases
            }
            if ($script:ReferenceAllCases -notcontains $token) {
                throw "Unsupported case id: $token"
            }
            if (-not $expanded.Contains($token)) {
                $expanded.Add($token)
            }
        }
    }

    return @($expanded)
}

function New-ReferenceContext {
    param(
        [string]$Root = (Get-ReferenceRoot),
        [string[]]$Cases,
        [switch]$Smoke
    )

    $timestamp = Get-Date -Format 'yyyyMMdd-HHmmss'
    $savedRoot = Ensure-ReferenceDirectory -Path (Join-Path $Root 'Saved\ReferenceCases')
    $runRoot = Ensure-ReferenceDirectory -Path (Join-Path $savedRoot $timestamp)
    $logsDir = Ensure-ReferenceDirectory -Path (Join-Path $runRoot 'logs')
    $reportsDir = Ensure-ReferenceDirectory -Path (Join-Path $runRoot 'reports')
    $externalsDir = Ensure-ReferenceDirectory -Path (Join-Path $savedRoot 'externals')
    $datasetsDir = Ensure-ReferenceDirectory -Path (Join-Path $savedRoot 'datasets')

    return [pscustomobject]@{
        Root          = $Root
        Timestamp     = $timestamp
        SavedRoot     = $savedRoot
        RunRoot       = $runRoot
        LogsDir       = $logsDir
        ReportsDir    = $reportsDir
        ExternalsDir  = $externalsDir
        DatasetsDir   = $datasetsDir
        Cases         = @(Resolve-ReferenceCases -Cases $Cases)
        Smoke         = [bool]$Smoke
        EnvName       = 'gaspals_ref_py311'
        PythonVersion = '3.11'
    }
}

function New-ReferenceResult {
    param(
        [Parameter(Mandatory = $true)]
        [string]$CaseId,
        [Parameter(Mandatory = $true)]
        [string]$Stage,
        [string]$Status = 'PENDING',
        [string]$Summary = '',
        [string[]]$Artifacts = @(),
        [string[]]$Notes = @(),
        [string]$LogPath = '',
        [string]$DataPath = ''
    )

    return [pscustomobject]@{
        CaseId    = $CaseId
        Stage     = $Stage
        Status    = $Status
        Summary   = $Summary
        Artifacts = @($Artifacts)
        Notes     = @($Notes)
        LogPath   = $LogPath
        DataPath  = $DataPath
    }
}

function Get-ReferenceLogPath {
    param(
        [Parameter(Mandatory = $true)]
        [pscustomobject]$Context,
        [Parameter(Mandatory = $true)]
        [string]$Name
    )

    return Join-Path $Context.LogsDir "$Name.log"
}

function Resolve-ReferenceCommand {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Name
    )

    $command = Get-Command $Name -ErrorAction SilentlyContinue
    if ($null -eq $command) {
        return $null
    }
    return $command.Source
}

function Get-ReferenceCondaExecutable {
    $candidates = New-Object System.Collections.Generic.List[string]

    try {
        $whereResults = & where.exe conda 2>$null
        foreach ($path in @($whereResults)) {
            if (-not [string]::IsNullOrWhiteSpace($path)) {
                $candidates.Add($path.Trim())
            }
        }
    }
    catch {
    }

    foreach ($path in @(
        'C:\ProgramData\anaconda3\Scripts\conda.exe',
        'C:\ProgramData\anaconda3\condabin\conda.bat',
        'C:\ProgramData\anaconda3\Library\bin\conda.bat',
        "$env:USERPROFILE\miniconda3\Scripts\conda.exe",
        "$env:USERPROFILE\miniconda3\condabin\conda.bat",
        "$env:USERPROFILE\anaconda3\Scripts\conda.exe",
        "$env:USERPROFILE\anaconda3\condabin\conda.bat"
    )) {
        $candidates.Add($path)
    }

    $resolved = Resolve-ReferenceCommand -Name 'conda'
    if ($resolved) {
        $candidates.Add($resolved)
    }

    $orderedCandidates = @(
        $candidates |
        Select-Object -Unique |
        Sort-Object {
            if ($_ -like '*.exe') { 0 }
            elseif ($_ -like '*.bat') { 1 }
            else { 2 }
        }
    )

    foreach ($candidate in $orderedCandidates) {
        if ((Test-Path $candidate) -and $candidate -match 'conda(\.exe|\.bat)$') {
            return $candidate
        }
    }

    throw 'conda executable was not found on PATH.'
}

function Get-ReferenceMsBuildPath {
    $path = Resolve-ReferenceCommand -Name 'msbuild'
    if ($path) {
        return $path
    }

    $candidates = @(
        'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe',
        'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\amd64\MSBuild.exe',
        'C:\Program Files\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe',
        'C:\Program Files\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\amd64\MSBuild.exe'
    )

    return $candidates | Where-Object { Test-Path $_ } | Select-Object -First 1
}

function Get-ReferenceUnrealEditors {
    $roots = @(
        'C:\Program Files\Epic Games',
        'D:\Program Files\Epic Games',
        'E:\Program Files\Epic Games',
        'G:\Game',
        'D:\Game',
        'D:\Engines'
    ) | Where-Object { Test-Path $_ }

    $items = New-Object System.Collections.Generic.List[object]
    foreach ($root in $roots) {
        foreach ($dir in Get-ChildItem -Path $root -Directory -Filter 'UE_*' -ErrorAction SilentlyContinue) {
            $editor = Join-Path $dir.FullName 'Engine\Binaries\Win64\UnrealEditor.exe'
            if (Test-Path $editor) {
                $version = $dir.Name -replace '^UE_', ''
                $items.Add([pscustomobject]@{
                    Version = $version
                    Root    = $dir.FullName
                    Editor  = $editor
                })
            }
        }
    }

    return @($items | Sort-Object Version)
}

function Get-ReferenceUnityEditors {
    $patterns = @(
        'C:\Program Files\Unity*\Editor\Unity.exe',
        'D:\Program Files\Unity*\Editor\Unity.exe',
        'C:\Program Files\Unity\Hub\Editor\*\Editor\Unity.exe',
        'D:\Program Files\Unity\Hub\Editor\*\Editor\Unity.exe'
    )

    $items = New-Object System.Collections.Generic.List[object]
    foreach ($pattern in $patterns) {
        foreach ($path in Get-ChildItem -Path $pattern -File -ErrorAction SilentlyContinue) {
            $version = (Split-Path $path.Directory.FullName -Parent | Split-Path -Leaf) -replace '^Unity\s+', ''
            $items.Add([pscustomobject]@{
                Version = $version
                Editor  = $path.FullName
            })
        }
    }

    return @($items | Sort-Object Version -Unique)
}

function Get-ReferenceDefaultDevice {
    if (Resolve-ReferenceCommand -Name 'nvidia-smi') {
        return 'cuda'
    }
    return 'cpu'
}

function Get-ReferenceGamepadNames {
    try {
        $devices = Get-CimInstance Win32_PnPEntity -ErrorAction Stop |
            Where-Object {
                $_.Name -match 'Xbox|Gamepad|DualSense|DualShock|Oray Virtual Game Controller'
            } |
            Select-Object -ExpandProperty Name
        return @($devices)
    }
    catch {
        return @()
    }
}

function Get-ReferenceGpuNames {
    try {
        return @(
            Get-CimInstance Win32_VideoController -ErrorAction Stop |
            Where-Object { -not [string]::IsNullOrWhiteSpace($_.Name) } |
            Select-Object -ExpandProperty Name
        )
    }
    catch {
        return @()
    }
}

function Get-ReferencePreflight {
    param(
        [Parameter(Mandatory = $true)]
        [pscustomobject]$Context
    )

    $ue = Get-ReferenceUnrealEditors
    $unity = Get-ReferenceUnityEditors

    return [pscustomobject]@{
        conda          = [bool](Resolve-ReferenceCommand -Name 'conda')
        uv             = [bool](Resolve-ReferenceCommand -Name 'uv')
        git_lfs        = [bool](Resolve-ReferenceCommand -Name 'git-lfs')
        msbuild        = [bool](Get-ReferenceMsBuildPath)
        unity2021      = [bool]($unity | Where-Object { $_.Version -like '2021.1.22f1c1*' })
        ue53           = [bool]($ue | Where-Object { $_.Version -like '5.3*' })
        ue54           = [bool]($ue | Where-Object { $_.Version -like '5.4*' })
        ue55           = [bool]($ue | Where-Object { $_.Version -like '5.5*' })
        ue57           = [bool]($ue | Where-Object { $_.Version -like '5.7*' })
        gpp            = [bool](Resolve-ReferenceCommand -Name 'g++')
        make           = [bool](Resolve-ReferenceCommand -Name 'make')
        raylib         = Test-Path 'D:\raylib'
        gpu_names      = @(Get-ReferenceGpuNames)
        gamepad_names  = @(Get-ReferenceGamepadNames)
        fbxsdk_root    = $env:FBXSDK_ROOT
    }
}

function Write-ReferenceMarkdown {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path,
        [Parameter(Mandatory = $true)]
        [AllowEmptyString()]
        [string[]]$Lines
    )

    $parent = Split-Path $Path -Parent
    if ($parent) {
        Ensure-ReferenceDirectory -Path $parent | Out-Null
    }
    $Lines | Set-Content -Path $Path -Encoding UTF8
    return $Path
}

function Write-ReferencePreflightReport {
    param(
        [Parameter(Mandatory = $true)]
        [pscustomobject]$Context,
        [Parameter(Mandatory = $true)]
        [pscustomobject]$Preflight
    )

    $reportPath = Join-Path $Context.ReportsDir 'preflight.md'
    $lines = @(
        '# ReferenceWalkthrough Preflight',
        '',
        "| Check | Value |",
        "| --- | --- |",
        "| conda | $($Preflight.conda) |",
        "| uv | $($Preflight.uv) |",
        "| git-lfs | $($Preflight.git_lfs) |",
        "| MSBuild | $($Preflight.msbuild) |",
        "| Unity 2021.1.22f1c1 | $($Preflight.unity2021) |",
        "| UE 5.3 | $($Preflight.ue53) |",
        "| UE 5.4 | $($Preflight.ue54) |",
        "| UE 5.5 | $($Preflight.ue55) |",
        "| UE 5.7 | $($Preflight.ue57) |",
        "| g++ | $($Preflight.gpp) |",
        "| make | $($Preflight.make) |",
        "| C:\\raylib | $($Preflight.raylib) |",
        "| GPU | $(([string]::Join('; ', $Preflight.gpu_names))) |",
        "| Gamepad | $(([string]::Join('; ', $Preflight.gamepad_names))) |",
        "| FBXSDK_ROOT | $($Preflight.fbxsdk_root) |"
    )

    Write-ReferenceMarkdown -Path $reportPath -Lines $lines | Out-Null
    return $reportPath
}

function Write-ReferenceSummaryReport {
    param(
        [Parameter(Mandatory = $true)]
        [pscustomobject]$Context,
        [Parameter(Mandatory = $true)]
        [object[]]$Results,
        [string]$Name = 'summary'
    )

    $reportPath = Join-Path $Context.ReportsDir "$Name.md"
    $lines = @(
        '# ReferenceWalkthrough Summary',
        '',
        "| Case | Stage | Status | Summary | Log | Data |",
        "| --- | --- | --- | --- | --- | --- |"
    )

    foreach ($result in $Results) {
        $summaryValue = if ($null -ne $result.Summary) { [string]$result.Summary } else { '' }
        $logValue = if ($null -ne $result.LogPath) { [string]$result.LogPath } else { '' }
        $dataValue = if ($null -ne $result.DataPath) { [string]$result.DataPath } else { '' }
        $summary = $summaryValue -replace '\|', '/'
        $log = $logValue -replace '\|', '/'
        $data = $dataValue -replace '\|', '/'
        $lines += "| $($result.CaseId) | $($result.Stage) | $($result.Status) | $summary | $log | $data |"
        foreach ($note in @($result.Notes)) {
            if (-not [string]::IsNullOrWhiteSpace($note)) {
                $lines += ''
                $lines += "- [$($result.CaseId)/$($result.Stage)] $note"
            }
        }
        foreach ($artifact in @($result.Artifacts)) {
            if (-not [string]::IsNullOrWhiteSpace($artifact)) {
                $lines += "- Artifact: $artifact"
            }
        }
        $lines += ''
    }

    Write-ReferenceMarkdown -Path $reportPath -Lines $lines | Out-Null
    return $reportPath
}

function Ensure-ReferenceCondaEnv {
    param(
        [Parameter(Mandatory = $true)]
        [pscustomobject]$Context
    )

    $conda = Get-ReferenceCondaExecutable

    $output = & $conda env list
    $exists = $false
    foreach ($line in $output) {
        if ($line -match "^\s*([^\s]+)?\s*\*?\s+(.+[\\\/]$($Context.EnvName))$") {
            $exists = $true
            break
        }
    }

    if (-not $exists) {
        & $conda create -y -n $Context.EnvName "python=$($Context.PythonVersion)"
        if ($LASTEXITCODE -ne 0) {
            throw "Failed to create conda environment: $($Context.EnvName)"
        }
    }

    return $Context.EnvName
}

function Get-ReferenceCondaEnvPath {
    param(
        [Parameter(Mandatory = $true)]
        [pscustomobject]$Context
    )

    $conda = Get-ReferenceCondaExecutable

    $output = & $conda env list
    foreach ($line in $output) {
        if ($line -match "^\s*([^\s]+)?\s*\*?\s+(.+[\\\/]$($Context.EnvName))$") {
            return $matches[2].Trim()
        }
    }

    throw "Conda environment path could not be resolved for $($Context.EnvName)"
}

function Get-ReferenceCondaPythonPath {
    param(
        [Parameter(Mandatory = $true)]
        [pscustomobject]$Context
    )

    $envPath = Get-ReferenceCondaEnvPath -Context $Context
    $pythonPath = Join-Path $envPath 'python.exe'
    if (-not (Test-Path $pythonPath)) {
        throw "python.exe was not found in conda environment: $envPath"
    }
    return $pythonPath
}

function Invoke-ReferenceCommand {
    param(
        [Parameter(Mandatory = $true)]
        [string]$FilePath,
        [string[]]$ArgumentList = @(),
        [string]$WorkingDirectory = '',
        [string]$LogPath = '',
        [hashtable]$Environment = @{},
        [switch]$IgnoreExitCode
    )

    if ($LogPath) {
        Ensure-ReferenceDirectory -Path (Split-Path $LogPath -Parent) | Out-Null
    }

    $previous = @{}
    foreach ($key in $Environment.Keys) {
        $previous[$key] = [Environment]::GetEnvironmentVariable($key, 'Process')
        [Environment]::SetEnvironmentVariable($key, [string]$Environment[$key], 'Process')
    }

    $output = @()
    $stdoutPath = ''
    $stderrPath = ''
    try {
        if ($WorkingDirectory) {
            Push-Location $WorkingDirectory
        }

        $stdoutPath = Join-Path ([System.IO.Path]::GetTempPath()) ("refcmd-" + [System.Guid]::NewGuid().ToString('N') + ".stdout.log")
        $stderrPath = Join-Path ([System.IO.Path]::GetTempPath()) ("refcmd-" + [System.Guid]::NewGuid().ToString('N') + ".stderr.log")
        $processWorkingDirectory = if ($WorkingDirectory) { $WorkingDirectory } else { (Get-Location).Path }

        $startProcessArgs = @{
            FilePath               = $FilePath
            WorkingDirectory       = $processWorkingDirectory
            RedirectStandardOutput = $stdoutPath
            RedirectStandardError  = $stderrPath
            Wait                   = $true
            PassThru               = $true
        }
        if ($ArgumentList.Count -gt 0) {
            $startProcessArgs['ArgumentList'] = $ArgumentList
        }

        $process = Start-Process @startProcessArgs

        $stdout = if (Test-Path $stdoutPath) { @(Get-Content -Path $stdoutPath -ErrorAction SilentlyContinue) } else { @() }
        $stderr = if (Test-Path $stderrPath) { @(Get-Content -Path $stderrPath -ErrorAction SilentlyContinue) } else { @() }
        $capturedOutput = @($stdout + $stderr)
        if ($LogPath) {
            if ($capturedOutput.Count -gt 0) {
                $capturedOutput | Set-Content -Path $LogPath -Encoding UTF8
            }
            else {
                New-Item -ItemType File -Path $LogPath -Force | Out-Null
            }
            $output = $capturedOutput
        }
        else {
            $output = $capturedOutput
        }
        $exitCode = if ($null -ne $process.ExitCode) { $process.ExitCode } else { 0 }
    }
    finally {
        if ($WorkingDirectory) {
            Pop-Location
        }
        foreach ($key in $Environment.Keys) {
            [Environment]::SetEnvironmentVariable($key, $previous[$key], 'Process')
        }
        foreach ($path in @($stdoutPath, $stderrPath)) {
            if (-not [string]::IsNullOrWhiteSpace($path) -and (Test-Path $path)) {
                Remove-Item -Path $path -Force -ErrorAction SilentlyContinue
            }
        }
    }

    if (-not $IgnoreExitCode -and $exitCode -ne 0) {
        throw "Command failed with exit code ${exitCode}: $FilePath $([string]::Join(' ', $ArgumentList))"
    }

    return [pscustomobject]@{
        ExitCode = $exitCode
        Output   = @($output)
        LogPath  = $LogPath
    }
}

function Invoke-ReferencePowerShellFile {
    param(
        [Parameter(Mandatory = $true)]
        [string]$ScriptPath,
        [string[]]$ScriptArguments = @(),
        [string]$WorkingDirectory = '',
        [string]$LogPath = '',
        [hashtable]$Environment = @{},
        [switch]$IgnoreExitCode
    )

    $powershellExe = if (Test-Path (Join-Path $PSHOME 'powershell.exe')) {
        Join-Path $PSHOME 'powershell.exe'
    }
    else {
        Resolve-ReferenceCommand -Name 'powershell'
    }

    return Invoke-ReferenceCommand `
        -FilePath $powershellExe `
        -ArgumentList (@('-ExecutionPolicy', 'Bypass', '-File', $ScriptPath) + $ScriptArguments) `
        -WorkingDirectory $WorkingDirectory `
        -LogPath $LogPath `
        -Environment $Environment `
        -IgnoreExitCode:$IgnoreExitCode
}

function Invoke-ReferenceConda {
    param(
        [Parameter(Mandatory = $true)]
        [pscustomobject]$Context,
        [Parameter(Mandatory = $true)]
        [string]$Executable,
        [string[]]$Arguments = @(),
        [string]$WorkingDirectory = '',
        [string]$LogPath = '',
        [hashtable]$Environment = @{},
        [switch]$IgnoreExitCode
    )

    $conda = Get-ReferenceCondaExecutable

    return Invoke-ReferenceCommand `
        -FilePath $conda `
        -ArgumentList (@('run', '--no-capture-output', '-n', $Context.EnvName, $Executable) + $Arguments) `
        -WorkingDirectory $WorkingDirectory `
        -LogPath $LogPath `
        -Environment $Environment `
        -IgnoreExitCode:$IgnoreExitCode
}

function Download-ReferenceFile {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Uri,
        [Parameter(Mandatory = $true)]
        [string]$Destination
    )

    Ensure-ReferenceDirectory -Path (Split-Path $Destination -Parent) | Out-Null
    Invoke-WebRequest -Uri $Uri -OutFile $Destination
    return $Destination
}

function Expand-ReferenceZip {
    param(
        [Parameter(Mandatory = $true)]
        [string]$ArchivePath,
        [Parameter(Mandatory = $true)]
        [string]$Destination
    )

    Ensure-ReferenceDirectory -Path $Destination | Out-Null
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $destinationRoot = [System.IO.Path]::GetFullPath($Destination)
    $archive = [System.IO.Compression.ZipFile]::OpenRead($ArchivePath)
    try {
        foreach ($entry in $archive.Entries) {
            if ([string]::IsNullOrWhiteSpace($entry.FullName)) {
                continue
            }

            $targetPath = [System.IO.Path]::GetFullPath((Join-Path $destinationRoot $entry.FullName))
            if (-not $targetPath.StartsWith($destinationRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
                throw "Zip entry attempted to escape destination root: $($entry.FullName)"
            }

            if ([string]::IsNullOrEmpty($entry.Name)) {
                Ensure-ReferenceDirectory -Path $targetPath | Out-Null
                continue
            }

            $parent = Split-Path $targetPath -Parent
            if ($parent) {
                Ensure-ReferenceDirectory -Path $parent | Out-Null
            }

            $inputStream = $entry.Open()
            try {
                $outputStream = [System.IO.File]::Open($targetPath, [System.IO.FileMode]::Create, [System.IO.FileAccess]::Write, [System.IO.FileShare]::None)
                try {
                    $inputStream.CopyTo($outputStream)
                }
                finally {
                    $outputStream.Dispose()
                }
            }
            finally {
                $inputStream.Dispose()
            }

            [System.IO.File]::SetLastWriteTime($targetPath, $entry.LastWriteTime.DateTime)
        }
    }
    finally {
        $archive.Dispose()
    }

    return $Destination
}

function Ensure-ReferenceGitClone {
    param(
        [Parameter(Mandatory = $true)]
        [string]$RepositoryUrl,
        [Parameter(Mandatory = $true)]
        [string]$Destination,
        [switch]$Pull
    )

    $git = Resolve-ReferenceCommand -Name 'git'
    if (-not $git) {
        throw 'git was not found on PATH.'
    }

    if (Test-Path (Join-Path $Destination '.git')) {
        if ($Pull) {
            Invoke-ReferenceCommand -FilePath $git -ArgumentList @('-C', $Destination, 'pull', '--ff-only')
        }
    }
    else {
        Ensure-ReferenceDirectory -Path (Split-Path $Destination -Parent) | Out-Null
        Invoke-ReferenceCommand -FilePath $git -ArgumentList @('clone', '--depth', '1', $RepositoryUrl, $Destination)
    }

    return $Destination
}

function Get-ReferenceUnrealEditor {
    param(
        [Parameter(Mandatory = $true)]
        [string[]]$PreferredVersions
    )

    $available = Get-ReferenceUnrealEditors
    foreach ($version in $PreferredVersions) {
        $match = $available | Where-Object { $_.Version -like "$version*" } | Select-Object -First 1
        if ($match) {
            return $match
        }
    }
    return $null
}

function Get-ReferenceUnityEditor {
    param(
        [string]$PreferredVersion = '2021.1.22f1c1'
    )

    return Get-ReferenceUnityEditors | Where-Object { $_.Version -like "$PreferredVersion*" } | Select-Object -First 1
}
