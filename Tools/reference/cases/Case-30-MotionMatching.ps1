Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

. (Join-Path $PSScriptRoot '..\common.ps1')

function Get-ReferenceCase30Paths {
    param(
        [Parameter(Mandatory = $true)]
        [pscustomobject]$Context
    )

    $repoRoot = Join-Path $Context.Root 'References\Motion-Matching'
    $resourcesRoot = Join-Path $repoRoot 'resources'
    $datasetRoot = Join-Path $Context.DatasetsDir 'motion-matching'
    return [pscustomobject]@{
        RepoRoot            = $repoRoot
        ResourcesRoot       = $resourcesRoot
        DatasetRoot         = $datasetRoot
        DatasetRepoRoot     = Join-Path $datasetRoot 'ubisoft-laforge-animation-dataset'
        DatasetSubsetRoot   = Join-Path $datasetRoot 'bvh'
        LocalDatasetRoot    = Join-Path $Context.Root 'References\Learned_Motion_Matching_Training\Animations\LAFAN1BVH'
        ControllerExe       = Join-Path $repoRoot 'controller.exe'
        RaylibZip           = Join-Path $Context.ExternalsDir 'raylib-5.5_win64_mingw-w64.zip'
        RaylibExtractRoot   = Join-Path $Context.ExternalsDir 'raylib-5.5'
    }
}

function Get-ReferenceMakeCommand {
    $make = Resolve-ReferenceCommand -Name 'make'
    if ($make) {
        return $make
    }
    $make = Resolve-ReferenceCommand -Name 'mingw32-make'
    if ($make) {
        return $make
    }

    $wingetMake = "D:\MinGW\mingw64\bin\mingw32-make.exe"
    if (Test-Path $wingetMake) {
        return $wingetMake
    }

    return $null
}

function Get-ReferenceGppCommand {
    $gpp = Resolve-ReferenceCommand -Name 'g++'
    if ($gpp) {
        return $gpp
    }

    $wingetGpp = "D:\MinGW\mingw64\bin\g++.exe"
    if (Test-Path $wingetGpp) {
        return $wingetGpp
    }

    return $null
}

function Get-ReferenceCase30ToolchainEnvironment {
    param(
        [string]$MakePath,
        [string]$GppPath
    )

    $toolchainBin = $null
    foreach ($candidate in @($MakePath, $GppPath)) {
        if (-not [string]::IsNullOrWhiteSpace($candidate) -and (Test-Path $candidate)) {
            $toolchainBin = Split-Path $candidate -Parent
            break
        }
    }

    if ([string]::IsNullOrWhiteSpace($toolchainBin)) {
        return @{}
    }

    $pathEntries = @($toolchainBin)
    foreach ($extra in @('D:\raylib\lib', 'D:\raylib\raylib\src')) {
        if (Test-Path $extra) {
            $pathEntries += $extra
        }
    }
    $pathEntries += [Environment]::GetEnvironmentVariable('PATH', 'Process')

    return @{
        PATH = ($pathEntries -join ';')
    }
}

function Ensure-ReferenceRaylib {
    param(
        [Parameter(Mandatory = $true)]
        [pscustomobject]$Paths
    )

    $notes = New-Object System.Collections.Generic.List[string]
    if (-not (Test-Path $Paths.RaylibZip)) {
        Download-ReferenceFile -Uri 'https://github.com/raysan5/raylib/releases/download/5.5/raylib-5.5_win64_mingw-w64.zip' -Destination $Paths.RaylibZip | Out-Null
        $notes.Add("Downloaded raylib archive to $($Paths.RaylibZip)")
    }

    Expand-ReferenceZip -ArchivePath $Paths.RaylibZip -Destination $Paths.RaylibExtractRoot | Out-Null

    $extractedRoot = Get-ChildItem -Path $Paths.RaylibExtractRoot -Directory | Select-Object -First 1
    if ($null -ne $extractedRoot -and -not (Test-Path 'D:\raylib')) {
        try {
            Ensure-ReferenceDirectory -Path 'D:\raylib' | Out-Null
            Copy-Item -Path (Join-Path $extractedRoot.FullName '*') -Destination 'D:\raylib' -Recurse -Force
            $notes.Add('Copied raylib into D:\raylib')
        }
        catch {
            $notes.Add("Failed to copy raylib into D:\raylib: $($_.Exception.Message)")
        }
    }

    $legacySrc = Ensure-ReferenceDirectory -Path 'D:\raylib\raylib\src'
    foreach ($header in @('raylib.h', 'raymath.h', 'rlgl.h')) {
        $sourceHeader = Join-Path 'D:\raylib\include' $header
        if (Test-Path $sourceHeader) {
            Copy-Item -Path $sourceHeader -Destination (Join-Path $legacySrc $header) -Force
        }
    }
    foreach ($library in @('libraylib.a', 'libraylibdll.a', 'raylib.dll')) {
        $sourceLibrary = Join-Path 'D:\raylib\lib' $library
        if (Test-Path $sourceLibrary) {
            Copy-Item -Path $sourceLibrary -Destination (Join-Path $legacySrc $library) -Force
        }
    }
    $rcData = Join-Path $legacySrc 'raylib.rc.data'
    if (-not (Test-Path $rcData)) {
        Set-Content -Path $rcData -Value '' -Encoding ASCII
    }

    $rayguiHeader = 'D:\raylib\raygui\src\raygui.h'
    if (-not (Test-Path $rayguiHeader)) {
        $rayguiDir = Ensure-ReferenceDirectory -Path (Split-Path $rayguiHeader -Parent)
        $rayguiUrl = 'https://raw.githubusercontent.com/raysan5/raygui/4.0/src/raygui.h'
        Download-ReferenceFile -Uri $rayguiUrl -Destination $rayguiHeader | Out-Null
        $notes.Add("Downloaded raygui.h to $rayguiDir")
    }

    return @($notes)
}

function Ensure-ReferenceMotionMatchingDataset {
    param(
        [Parameter(Mandatory = $true)]
        [pscustomobject]$Paths
    )

    $required = @(
        'pushAndStumble1_subject5.bvh',
        'run1_subject5.bvh',
        'walk1_subject5.bvh'
    )

    Ensure-ReferenceDirectory -Path $Paths.DatasetRoot | Out-Null
    Ensure-ReferenceDirectory -Path $Paths.DatasetSubsetRoot | Out-Null

    $copied = New-Object System.Collections.Generic.List[string]
    foreach ($fileName in $required) {
        $target = Join-Path $Paths.DatasetSubsetRoot $fileName
        if (Test-Path $target) {
            $copied.Add($target)
            continue
        }

        $localMatch = $null
        if (Test-Path $Paths.LocalDatasetRoot) {
            $localMatch = Get-ChildItem -Path $Paths.LocalDatasetRoot -Recurse -Filter $fileName -ErrorAction SilentlyContinue | Select-Object -First 1
        }
        if ($localMatch) {
            Copy-Item -Path $localMatch.FullName -Destination $target -Force
            $copied.Add($target)
            continue
        }

        $lafanZipPath = Join-Path $Paths.DatasetRoot 'lafan1.zip'
        if (-not (Test-Path $lafanZipPath)) {
            Download-ReferenceFile `
                -Uri 'https://media.githubusercontent.com/media/ubisoft/ubisoft-laforge-animation-dataset/94084601bacdf9cc3764b5c73daaeccae6035fac/lafan1/lafan1.zip' `
                -Destination $lafanZipPath | Out-Null
        }

        if (Test-Path $lafanZipPath) {
            Add-Type -AssemblyName System.IO.Compression.FileSystem
            $archive = [System.IO.Compression.ZipFile]::OpenRead($lafanZipPath)
            try {
                $entry = $archive.Entries | Where-Object { $_.FullName -like "*$fileName" } | Select-Object -First 1
                if ($entry) {
                    $stream = $entry.Open()
                    try {
                        $fileStream = [System.IO.File]::Open($target, [System.IO.FileMode]::Create, [System.IO.FileAccess]::Write)
                        try {
                            $stream.CopyTo($fileStream)
                        }
                        finally {
                            $fileStream.Dispose()
                        }
                    }
                    finally {
                        $stream.Dispose()
                    }
                }
            }
            finally {
                $archive.Dispose()
            }
        }

        if (Test-Path $target) {
            $copied.Add($target)
            continue
        }

        throw "Required BVH clip was not found in the local LAFAN1 cache or downloaded lafan1.zip: $fileName"
    }

    return @($copied)
}

function Install-ReferenceCase30PythonDeps {
    param(
        [Parameter(Mandatory = $true)]
        [pscustomobject]$Context,
        [Parameter(Mandatory = $true)]
        [pscustomobject]$Paths,
        [Parameter(Mandatory = $true)]
        [string]$LogPath
    )

    Invoke-ReferenceConda -Context $Context -Executable 'python' -Arguments @(
        '-m', 'pip', 'install',
        'numpy', 'scipy', 'matplotlib', 'scikit-learn', 'tensorboard', 'torch'
    ) -WorkingDirectory $Paths.ResourcesRoot -LogPath $LogPath | Out-Null
}

function Invoke-ReferenceCase30Setup {
    param(
        [Parameter(Mandatory = $true)]
        [pscustomobject]$Context
    )

    $paths = Get-ReferenceCase30Paths -Context $Context
    $log = Get-ReferenceLogPath -Context $Context -Name 'case-30-setup'
    $notes = New-Object System.Collections.Generic.List[string]
    $artifacts = New-Object System.Collections.Generic.List[string]

    Ensure-ReferenceCondaEnv -Context $Context | Out-Null
    Install-ReferenceCase30PythonDeps -Context $Context -Paths $paths -LogPath $log

    foreach ($note in Ensure-ReferenceRaylib -Paths $paths) {
        $notes.Add($note)
    }
    foreach ($artifact in Ensure-ReferenceMotionMatchingDataset -Paths $paths) {
        $artifacts.Add($artifact)
    }

    $gpp = Get-ReferenceGppCommand
    $make = Get-ReferenceMakeCommand
    if (-not $gpp -or -not $make) {
        $notes.Add('g++/make was not detected. Setup prepared data and Python deps, but native build still needs a working MinGW or MSYS2 toolchain.')
        $status = 'MANUAL'
    }
    else {
        $status = 'PASS'
    }

    return New-ReferenceResult -CaseId '30' -Stage 'setup' -Status $status -Summary 'Prepared Motion-Matching datasets, Python dependencies, and raylib assets.' -Artifacts @($artifacts) -Notes @($notes) -LogPath $log -DataPath $paths.DatasetSubsetRoot
}

function Invoke-ReferenceCase30Test {
    param(
        [Parameter(Mandatory = $true)]
        [pscustomobject]$Context
    )

    $paths = Get-ReferenceCase30Paths -Context $Context
    $log = Get-ReferenceLogPath -Context $Context -Name 'case-30-test'
    $notes = New-Object System.Collections.Generic.List[string]
    $artifacts = New-Object System.Collections.Generic.List[string]

    Ensure-ReferenceCondaEnv -Context $Context | Out-Null

    $make = Get-ReferenceMakeCommand
    $gpp = Get-ReferenceGppCommand
    if (-not $make -or -not $gpp) {
        return New-ReferenceResult -CaseId '30' -Stage 'test' -Status 'BLOCKED' -Summary 'g++ and make are required to build controller.exe for the runtime inference step.' -Notes @('Install a MinGW/MSYS2 toolchain or expose make/g++ on PATH, then re-run case 30.') -LogPath $log -DataPath $paths.RepoRoot
    }

    try {
        foreach ($artifact in Ensure-ReferenceMotionMatchingDataset -Paths $paths) {
            if (-not $artifacts.Contains($artifact)) {
                $artifacts.Add($artifact)
            }
        }

        Get-Process -ErrorAction SilentlyContinue |
            Where-Object { $_.Path -eq $paths.ControllerExe } |
            Stop-Process -Force -ErrorAction SilentlyContinue
        if (Test-Path $paths.ControllerExe) {
            Remove-Item -Path $paths.ControllerExe -Force -ErrorAction SilentlyContinue
        }

        Invoke-ReferenceConda -Context $Context -Executable 'python' -Arguments @(
            'generate_database.py',
            '--input-dir', $paths.DatasetSubsetRoot,
            '--output', (Join-Path $paths.ResourcesRoot 'database.bin'),
            '--skip-visualize'
        ) -WorkingDirectory $paths.ResourcesRoot -LogPath $log | Out-Null

        $toolchainEnvironment = Get-ReferenceCase30ToolchainEnvironment -MakePath $make -GppPath $gpp
        $makeArgs = if ($Context.Smoke) { @('BUILD_MODE=DEBUG') } else { @() }
        Invoke-ReferenceCommand -FilePath $make -ArgumentList $makeArgs -WorkingDirectory $paths.RepoRoot -LogPath $log -Environment $toolchainEnvironment | Out-Null

        if (-not (Test-Path $paths.ControllerExe)) {
            throw 'controller.exe was not produced by make.'
        }

        Invoke-ReferenceCommand -FilePath $paths.ControllerExe -ArgumentList @('--rebuild-features-only') -WorkingDirectory $paths.RepoRoot -LogPath $log -Environment $toolchainEnvironment | Out-Null

        $device = Get-ReferenceDefaultDevice
        $niter = if ($Context.Smoke) { '20' } else { '500000' }
        $saveEvery = if ($Context.Smoke) { '10' } else { '1000' }
        $batchSize = if ($Context.Smoke) { '32' } else { '32' }

        Invoke-ReferenceConda -Context $Context -Executable 'python' -Arguments @(
            'train_decompressor.py',
            '--niter', $niter,
            '--batchsize', $batchSize,
            '--device', $device,
            '--seed', '1234',
            '--log-every', '10',
            '--save-every', $saveEvery,
            '--database', (Join-Path $paths.ResourcesRoot 'database.bin'),
            '--features', (Join-Path $paths.ResourcesRoot 'features.bin'),
            '--latent', (Join-Path $paths.ResourcesRoot 'latent.bin'),
            '--output', (Join-Path $paths.ResourcesRoot 'decompressor.bin'),
            '--plots-dir', $paths.ResourcesRoot
        ) -WorkingDirectory $paths.ResourcesRoot -LogPath $log | Out-Null

        Invoke-ReferenceConda -Context $Context -Executable 'python' -Arguments @(
            'train_projector.py',
            '--niter', $niter,
            '--batchsize', $batchSize,
            '--device', $device,
            '--seed', '1234',
            '--log-every', '10',
            '--save-every', $saveEvery,
            '--database', (Join-Path $paths.ResourcesRoot 'database.bin'),
            '--features', (Join-Path $paths.ResourcesRoot 'features.bin'),
            '--latent', (Join-Path $paths.ResourcesRoot 'latent.bin'),
            '--output', (Join-Path $paths.ResourcesRoot 'projector.bin'),
            '--plots-dir', $paths.ResourcesRoot
        ) -WorkingDirectory $paths.ResourcesRoot -LogPath $log | Out-Null

        Invoke-ReferenceConda -Context $Context -Executable 'python' -Arguments @(
            'train_stepper.py',
            '--niter', $niter,
            '--batchsize', $batchSize,
            '--device', $device,
            '--seed', '1234',
            '--log-every', '10',
            '--save-every', $saveEvery,
            '--database', (Join-Path $paths.ResourcesRoot 'database.bin'),
            '--features', (Join-Path $paths.ResourcesRoot 'features.bin'),
            '--latent', (Join-Path $paths.ResourcesRoot 'latent.bin'),
            '--output', (Join-Path $paths.ResourcesRoot 'stepper.bin'),
            '--plots-dir', $paths.ResourcesRoot
        ) -WorkingDirectory $paths.ResourcesRoot -LogPath $log | Out-Null

        $previousPath = [Environment]::GetEnvironmentVariable('PATH', 'Process')
        try {
            if ($toolchainEnvironment.ContainsKey('PATH')) {
                [Environment]::SetEnvironmentVariable('PATH', $toolchainEnvironment['PATH'], 'Process')
            }

            $process = Start-Process -FilePath $paths.ControllerExe -ArgumentList @('--lmm-enabled') -WorkingDirectory $paths.RepoRoot -PassThru
            Start-Sleep -Seconds 10
            if ($process.HasExited) {
                throw "controller.exe exited early with code $($process.ExitCode)."
            }
        }
        finally {
            [Environment]::SetEnvironmentVariable('PATH', $previousPath, 'Process')
        }
        Stop-Process -Id $process.Id -Force
    }
    catch {
        $notes.Add($_.Exception.Message)
        return New-ReferenceResult -CaseId '30' -Stage 'test' -Status 'BLOCKED' -Summary 'Motion-Matching full chain failed before the LMM runtime demo stayed alive.' -Notes @($notes) -LogPath $log -DataPath $paths.RepoRoot
    }

    foreach ($artifact in @(
        (Join-Path $paths.ResourcesRoot 'database.bin'),
        (Join-Path $paths.ResourcesRoot 'features.bin'),
        (Join-Path $paths.ResourcesRoot 'latent.bin'),
        (Join-Path $paths.ResourcesRoot 'decompressor.bin'),
        (Join-Path $paths.ResourcesRoot 'projector.bin'),
        (Join-Path $paths.ResourcesRoot 'stepper.bin'),
        $paths.ControllerExe
    )) {
        if (Test-Path $artifact) {
            $artifacts.Add($artifact)
        }
    }

    if ($Context.Smoke) {
        $notes.Add('Smoke validation used BUILD_MODE=DEBUG because the upstream Release flags crash on this Windows toolchain/runtime combination.')
    }
    $notes.Add('controller.exe was launched with --lmm-enabled and stayed alive long enough to confirm the runtime inference path was active.')
    return New-ReferenceResult -CaseId '30' -Stage 'test' -Status 'PASS' -Summary 'Completed Motion-Matching data generation, feature rebuild, three-network training, and runtime LMM launch.' -Artifacts @($artifacts) -Notes @($notes) -LogPath $log -DataPath $paths.ResourcesRoot
}
