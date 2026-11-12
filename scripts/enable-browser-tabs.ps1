[CmdletBinding()]
param(
    [string] $Executable = (Join-Path $env:LOCALAPPDATA 'Zenula\Zenula.exe')
)

$ErrorActionPreference = 'Stop'
$exe = (Get-Item -LiteralPath $Executable).FullName
$folder = Split-Path -Parent $exe
$hostName = 'com.zenith_dot.browser'
$manifestPath = Join-Path $folder ($hostName + '.json')
$manifest = @{
    name = $hostName
    description = 'Zenula browser media bridge'
    path = $exe
    type = 'stdio'
    allowed_origins = @('chrome-extension://achehkjcdkfodmalnalebhpafbonlccf/')
} | ConvertTo-Json
[IO.File]::WriteAllText($manifestPath, $manifest, [Text.UTF8Encoding]::new($false))

foreach ($vendor in @('Google\Chrome', 'Microsoft\Edge', 'BraveSoftware\Brave-Browser')) {
    $location = "HKCU:\Software\$vendor\NativeMessagingHosts\$hostName"
    New-Item -Path $location -Force | Out-Null
    Set-Item -Path $location -Value $manifestPath
}
Write-Output 'Zenula browser bridge registered for this Windows user.'
