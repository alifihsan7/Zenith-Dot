[CmdletBinding()]
param(
    [string] $Executable = (Join-Path $PSScriptRoot 'Zenula.exe')
)

$ErrorActionPreference = 'Stop'
$source = (Get-Item -LiteralPath $Executable).FullName
$installDir = Join-Path $env:LOCALAPPDATA 'Zenula'
$installedExe = Join-Path $installDir 'Zenula.exe'
$legacyDir = Join-Path $env:LOCALAPPDATA 'Zenith Dot'
$legacyExe = Join-Path $legacyDir 'ZenithDot.exe'
$firstInstall = !(Test-Path -LiteralPath $installedExe) -and !(Test-Path -LiteralPath $legacyExe)
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
$legacyStartup = Join-Path $startup 'Zenith Dot.lnk'
$legacyShortcuts = @(
    foreach ($path in @($legacyStartup, (Join-Path $programs 'Zenith Dot.lnk'), (Join-Path $programs 'Zenith Dot Settings.lnk'))) {
        if (!(Test-Path -LiteralPath $path -PathType Leaf)) { continue }
        $target = $shell.CreateShortcut($path).TargetPath
        if ([IO.Path]::GetFileName($target) -eq 'ZenithDot.exe') { $path }
    }
)
$shortcutFolders = @($programs)
if ($firstInstall -or (Test-Path -LiteralPath (Join-Path $startup 'Zenula.lnk')) -or
    ($legacyShortcuts -contains $legacyStartup)) {
    $shortcutFolders += $startup
}
foreach ($folder in $shortcutFolders) {
    $shortcut = $shell.CreateShortcut((Join-Path $folder 'Zenula.lnk'))
    $shortcut.TargetPath = $installedExe
    $shortcut.IconLocation = "$installedExe,0"
    $shortcut.WorkingDirectory = $installDir
    $shortcut.Description = 'Zenula'
    $shortcut.Save()
}

$settingsShortcut = $shell.CreateShortcut((Join-Path $programs 'Zenula Settings.lnk'))
$settingsShortcut.TargetPath = $installedExe
$settingsShortcut.Arguments = '--settings'
$settingsShortcut.IconLocation = "$installedExe,0"
$settingsShortcut.WorkingDirectory = $installDir
$settingsShortcut.Description = 'Zenula Settings'
$settingsShortcut.Save()

foreach ($path in $legacyShortcuts) { Remove-Item -LiteralPath $path }

$legacyManifest = Join-Path $legacyDir 'com.zenith_dot.browser.json'
if (Test-Path -LiteralPath $legacyManifest -PathType Leaf) {
    $bridge = Get-Content -LiteralPath $legacyManifest -Raw | ConvertFrom-Json
    if ($bridge.path -eq $legacyExe) {
        $bridge.path = $installedExe
        [IO.File]::WriteAllText($legacyManifest, ($bridge | ConvertTo-Json), [Text.UTF8Encoding]::new($false))
    }
}

Write-Output 'Zenula and Zenula Settings are installed in your Start menu.'
