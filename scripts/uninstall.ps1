[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$installDir = Join-Path $env:LOCALAPPDATA 'Zenith Dot'
$installedExe = Join-Path $installDir 'ZenithDot.exe'
if (Test-Path -LiteralPath $installedExe) {
    Remove-Item -LiteralPath $installedExe
}

$hostName = 'com.zenith_dot.browser'
foreach ($vendor in @('Google\Chrome', 'Microsoft\Edge')) {
    $location = "HKCU:\Software\$vendor\NativeMessagingHosts\$hostName"
    if (Test-Path -LiteralPath $location) {
        Remove-Item -LiteralPath $location
    }
}
$manifestPath = Join-Path $installDir ($hostName + '.json')
if (Test-Path -LiteralPath $manifestPath) {
    Remove-Item -LiteralPath $manifestPath
}

foreach ($location in @('Programs', 'Startup')) {
    $folder = [Environment]::GetFolderPath($location)
    if ($folder) {
        $shortcut = Join-Path $folder 'Zenith Dot.lnk'
        if (Test-Path -LiteralPath $shortcut) {
            Remove-Item -LiteralPath $shortcut
        }
    }
}
if ((Test-Path -LiteralPath $installDir) -and !(Get-ChildItem -LiteralPath $installDir -Force)) {
    Remove-Item -LiteralPath $installDir
}

Write-Output 'Zenith Dot was removed from your Start menu and Startup.'
