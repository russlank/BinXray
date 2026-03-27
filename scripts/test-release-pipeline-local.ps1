# SPDX-License-Identifier: MIT
param(
    [string]$Configuration = "Release",
    [string]$Platform = "x64",
    [string]$PlatformToolset = "v143",
    [string]$WixVersion = "6.0.2",
    [string]$OutputDir = "artifacts\release",
    [switch]$SkipPackaging
)

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
Push-Location $repoRoot

try {
    $version = python scripts/get_version.py
    if (-not $version) {
        throw "Failed to read version from scripts/get_version.py."
    }

    $solutionDir = Join-Path $repoRoot "src\"
    $intDir = Join-Path $repoRoot "build\obj\"
    $outDir = Join-Path $repoRoot "build\bin\"

    $repoUrl = (& git config --get remote.origin.url 2>$null)
    if ($repoUrl) {
        $repoUrl = $repoUrl.Trim()
        if ($repoUrl -match '^git@github\.com:(.+)\.git$') {
            $repoUrl = "https://github.com/$($Matches[1])"
        }
        elseif ($repoUrl -match '\.git$') {
            $repoUrl = $repoUrl.Substring(0, $repoUrl.Length - 4)
        }
    }
    if (-not $repoUrl) {
        $repoUrl = "unknown"
    }

    $branch = (& git rev-parse --abbrev-ref HEAD 2>$null)
    if ($branch) { $branch = $branch.Trim() }
    if (-not $branch) { $branch = "unknown" }

    $commit = (& git rev-parse HEAD 2>$null)
    if ($commit) { $commit = $commit.Trim() }
    if (-not $commit) { $commit = "unknown" }

    Write-Host "Version: $version"
    Write-Host "Configuration=$Configuration Platform=$Platform PlatformToolset=$PlatformToolset"
    Write-Host "Build metadata: repo=$repoUrl branch=$branch commit=$commit version=$version"

    & ".\scripts\invoke-msbuild.ps1" `
        -ProjectPath "src\BinXray\BinXray.vcxproj" `
        -Configuration $Configuration `
        -Platform $Platform `
        -Targets "Build" `
        -AdditionalMsBuildArgs @(
            "/p:PlatformToolset=$PlatformToolset",
            "/p:BxrBuildRepoUrl=$repoUrl",
            "/p:BxrBuildBranch=$branch",
            "/p:BxrBuildCommit=$commit",
            "/p:BxrBuildVersion=$version",
            "/p:SolutionDir=$solutionDir",
            "/p:IntDir=$intDir",
            "/p:OutDir=$outDir"
        )

    if ($SkipPackaging) {
        Write-Host "SkipPackaging was set. Build stage completed successfully."
        return
    }

    $wixCommand = Get-Command wix -ErrorAction SilentlyContinue
    if (-not $wixCommand) {
        throw "WiX CLI was not found in PATH. Install WiX 6.x: dotnet tool uninstall --global wix ; dotnet tool install --global wix --version $WixVersion ; wix extension add --global WixToolset.Bal.wixext/$WixVersion"
    }

    $wixInstalledVersion = (& wix --version).Trim()
    $wixMajor = $null
    if ($wixInstalledVersion -match '^(\d+)\.') {
        $wixMajor = [int]$Matches[1]
    }
    if ($wixMajor -ne 6) {
        throw "Unsupported WiX version '$wixInstalledVersion'. Install WiX 6.x: dotnet tool uninstall --global wix ; dotnet tool install --global wix --version $WixVersion ; wix extension add --global WixToolset.Bal.wixext/$WixVersion"
    }

    Write-Host "Using WiX: $wixInstalledVersion"

    & ".\packaging\build-packages.ps1" `
        -AppExePath "$repoRoot\build\bin\BinXray.exe" `
        -Version $version `
        -RequiredWixVersion $WixVersion `
        -OutputDir $OutputDir

    if ($LASTEXITCODE -ne 0) {
        throw "Packaging failed with exit code $LASTEXITCODE."
    }

    Write-Host "Local release pipeline simulation completed successfully."
    Write-Host "Artifacts: $(Join-Path $repoRoot $OutputDir)"
}
finally {
    Pop-Location
}
