[CmdletBinding()]
param(
    [string] $Executable = (Join-Path $PSScriptRoot 'ZenithDot.exe')
)

$ErrorActionPreference = 'Stop'
$source = (Get-Item -LiteralPath $Executable).FullName
$installDir = Join-Path $env:LOCALAPPDATA 'Zenith Dot'
$installedExe = Join-Path $installDir 'ZenithDot.exe'
$programs = [Environment]::GetFolderPath('Programs', 'Create')
$startup = [Environment]::GetFolderPath('Startup', 'Create')
if (!$programs -or !$startup) {
    throw 'Windows did not provide the Start menu and Startup folders.'
}

New-Item -ItemType Directory -Path $installDir -Force | Out-Null
if ($source -ne $installedExe) {
    Copy-Item -LiteralPath $source -Destination $installedExe -Force
}
$shell = New-Object -ComObject WScript.Shell
foreach ($folder in @($programs, $startup)) {
    $shortcut = $shell.CreateShortcut((Join-Path $folder 'Zenith Dot.lnk'))
    $shortcut.TargetPath = $installedExe
    $shortcut.WorkingDirectory = $installDir
    $shortcut.Description = 'Zenith Dot'
    $shortcut.Save()
}

Write-Output 'Zenith Dot is installed in your Start menu and will run at Windows sign-in.'
