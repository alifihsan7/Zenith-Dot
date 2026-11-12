[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$installDir = Join-Path $env:LOCALAPPDATA 'Zenula'
$installedExe = Join-Path $installDir 'Zenula.exe'
if (Test-Path -LiteralPath $installedExe) {
    Remove-Item -LiteralPath $installedExe
}

$hostName = 'com.zenith_dot.browser'
$manifestPath = Join-Path $installDir ($hostName + '.json')
$legacyManifest = Join-Path $env:LOCALAPPDATA ('Zenith Dot\' + $hostName + '.json')
$manifests = @($manifestPath)
if (Test-Path -LiteralPath $legacyManifest -PathType Leaf) {
    $bridge = Get-Content -LiteralPath $legacyManifest -Raw | ConvertFrom-Json
    if ($bridge.path -eq $installedExe) { $manifests += $legacyManifest }
}
foreach ($vendor in @('Google\Chrome', 'Microsoft\Edge', 'BraveSoftware\Brave-Browser')) {
    $location = "HKCU:\Software\$vendor\NativeMessagingHosts\$hostName"
    if ((Test-Path -LiteralPath $location) -and
        ($manifests -contains (Get-Item -LiteralPath $location).GetValue(''))) {
        Remove-Item -LiteralPath $location
    }
}
foreach ($path in $manifests) {
    if (Test-Path -LiteralPath $path -PathType Leaf) { Remove-Item -LiteralPath $path }
}

foreach ($location in @('Programs', 'Startup')) {
    $folder = [Environment]::GetFolderPath($location)
    if ($folder) {
        $shortcuts = @('Zenula.lnk')
        if ($location -eq 'Programs') { $shortcuts += 'Zenula Settings.lnk' }
        foreach ($name in $shortcuts) {
            $shortcut = Join-Path $folder $name
            if (Test-Path -LiteralPath $shortcut) {
                Remove-Item -LiteralPath $shortcut
            }
        }
    }
}
if ((Test-Path -LiteralPath $installDir) -and !(Get-ChildItem -LiteralPath $installDir -Force)) {
    Remove-Item -LiteralPath $installDir
}

Write-Output 'Zenula was removed from your Start menu and Startup.'
