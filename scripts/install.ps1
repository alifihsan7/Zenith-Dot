[CmdletBinding()]
param(
    [string] $Executable = (Join-Path $PSScriptRoot 'ZenithDot.exe')
)

$ErrorActionPreference = 'Stop'
$source = (Get-Item -LiteralPath $Executable).FullName
$installDir = Join-Path $env:LOCALAPPDATA 'Zenith Dot'
$installedExe = Join-Path $installDir 'ZenithDot.exe'
$firstInstall = !(Test-Path -LiteralPath $installedExe)
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
$shortcutFolders = @($programs)
if ($firstInstall -or (Test-Path -LiteralPath (Join-Path $startup 'Zenith Dot.lnk'))) {
    $shortcutFolders += $startup
}
foreach ($folder in $shortcutFolders) {
    $shortcut = $shell.CreateShortcut((Join-Path $folder 'Zenith Dot.lnk'))
    $shortcut.TargetPath = $installedExe
    $shortcut.IconLocation = "$installedExe,0"
    $shortcut.WorkingDirectory = $installDir
    $shortcut.Description = 'Zenith Dot'
    $shortcut.Save()
}

# Clear registrations from versions that shipped the browser companion.
foreach ($vendor in @('Google\Chrome', 'Microsoft\Edge', 'BraveSoftware\Brave-Browser')) {
    $location = "HKCU:\Software\$vendor\NativeMessagingHosts\com.zenith_dot.browser"
    if (Test-Path -LiteralPath $location) {
        Remove-Item -LiteralPath $location -ErrorAction Continue
    }
}

Write-Output 'Zenith Dot is installed in your Start menu. Use Settings in the tray to manage startup.'
