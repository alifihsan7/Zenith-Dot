[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$Executable,
    [Parameter(Mandatory)][string]$Output,
    [string]$Publisher = 'CN=Zenith Dot',
    [version]$Version = '0.1.0.0',
    [switch]$UnsignedTest
)

$ErrorActionPreference = 'Stop'
$source = (Resolve-Path -LiteralPath $Executable).Path
$package = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($Output)
$sdk = Join-Path ([Environment]::GetFolderPath('ProgramFilesX86')) 'Windows Kits\10\bin'
$makeAppx = Get-ChildItem -LiteralPath $sdk -Directory |
    Where-Object { $_.Name -match '^\d+\.\d+\.\d+\.\d+$' } |
    Sort-Object { [version]$_.Name } -Descending |
    ForEach-Object { Join-Path $_.FullName 'x64\makeappx.exe' } |
    Where-Object { Test-Path -LiteralPath $_ } |
    Select-Object -First 1
if (!$makeAppx) { throw 'Install the Windows SDK to build the MSIX package.' }

$stage = Join-Path ([IO.Path]::GetDirectoryName($package)) 'msix-assets'
New-Item -ItemType Directory -Path $stage -Force | Out-Null
if ($UnsignedTest) {
    $Publisher += ', OID.2.25.311729368913984317654407730594956997722=1'
}
$escapedPublisher = [Security.SecurityElement]::Escape($Publisher)
$manifest = Join-Path $stage 'AppxManifest.xml'
@"
<?xml version="1.0" encoding="utf-8"?>
<Package xmlns="http://schemas.microsoft.com/appx/manifest/foundation/windows10"
         xmlns:uap="http://schemas.microsoft.com/appx/manifest/uap/windows10"
         xmlns:uap3="http://schemas.microsoft.com/appx/manifest/uap/windows10/3"
         xmlns:rescap="http://schemas.microsoft.com/appx/manifest/foundation/windows10/restrictedcapabilities"
         IgnorableNamespaces="uap uap3 rescap">
  <Identity Name="ZenithDot" Publisher="$escapedPublisher" Version="$Version" ProcessorArchitecture="x64" />
  <Properties>
    <DisplayName>Zenula</DisplayName>
    <PublisherDisplayName>Zenula</PublisherDisplayName>
    <Logo>Assets\StoreLogo.png</Logo>
  </Properties>
  <Dependencies>
    <TargetDeviceFamily Name="Windows.Desktop" MinVersion="10.0.22000.0" MaxVersionTested="10.0.26100.0" />
  </Dependencies>
  <Resources>
    <Resource Language="en-us" />
  </Resources>
  <Applications>
    <Application Id="ZenithDot" Executable="Zenula.exe" EntryPoint="Windows.FullTrustApplication">
      <uap:VisualElements DisplayName="Zenula" Description="Zenula"
                         BackgroundColor="transparent"
                         Square150x150Logo="Assets\Square150x150Logo.png"
                         Square44x44Logo="Assets\Square44x44Logo.png" />
    </Application>
  </Applications>
  <Capabilities>
    <uap3:Capability Name="userNotificationListener" />
    <rescap:Capability Name="runFullTrust" />
  </Capabilities>
</Package>
"@ | Set-Content -LiteralPath $manifest -Encoding utf8

Add-Type -AssemblyName System.Drawing
$iconPath = Join-Path $PSScriptRoot '..\src\zenith.ico'
$icon = [Drawing.Icon]::new($iconPath, 256, 256)
try {
    $image = $icon.ToBitmap()
    try {
        $logos = [ordered]@{ StoreLogo = 50; Square150x150Logo = 150; Square44x44Logo = 44 }
        $mapping = @('[Files]', ('"{0}" "AppxManifest.xml"' -f $manifest), ('"{0}" "Zenula.exe"' -f $source))
        foreach ($name in $logos.Keys) {
            $size = $logos[$name]
            $bitmap = [Drawing.Bitmap]::new($size, $size)
            $graphics = [Drawing.Graphics]::FromImage($bitmap)
            try {
                $graphics.InterpolationMode = [Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
                $graphics.DrawImage($image, 0, 0, $size, $size)
                $file = Join-Path $stage "$name.png"
                $bitmap.Save($file, [Drawing.Imaging.ImageFormat]::Png)
                $mapping += '"{0}" "Assets\{1}.png"' -f $file, $name
            } finally {
                $graphics.Dispose()
                $bitmap.Dispose()
            }
        }
    } finally {
        $image.Dispose()
    }
} finally {
    $icon.Dispose()
}

$map = Join-Path $stage 'mapping.txt'
$mapping | Set-Content -LiteralPath $map -Encoding Unicode
& $makeAppx pack /o /f $map /p $package
if ($LASTEXITCODE -ne 0) { throw "MakeAppx failed with exit code $LASTEXITCODE." }
