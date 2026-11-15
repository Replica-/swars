<#
.SYNOPSIS
    Puts the two songs of the high definition music mode into the music folder of the game, as ogg files.

.DESCRIPTION
    The mode (the HIDEF MUSIC row of the pause menu) plays the files hidef_city.ogg - the normal music of the missions -
    and hidef_combat.ogg - the music of the fight, when the agents are under attack or an enemy who targets them is near.
    This script converts any two audio files you have (mp3, m4a, wav, ...) to these, with ffmpeg
    (winget install Gyan.FFmpeg). The songs are not part of the game: get them yourself.

.EXAMPLE
    ./tools/install-hidef-music.ps1 -City "C:\Music\city.mp3" -Combat "C:\Music\combat.mp3"
    ./tools/install-hidef-music.ps1 -City city.mp3 -Combat combat.mp3 -MusicDir "D:\Games\SyndWarsFX\music"
#>
param(
    [Parameter(Mandatory = $true)][string]$City,
    [Parameter(Mandatory = $true)][string]$Combat,
    [string]$MusicDir = "$PSScriptRoot\..\debug\src\music",
    [string]$Ffmpeg = "ffmpeg"
)

$ErrorActionPreference = "Stop"

$ffmpegCmd = Get-Command $Ffmpeg -ErrorAction SilentlyContinue
if (-not $ffmpegCmd) {
    # Just installed with winget, and the PATH of this session does not know it yet
    $found = @(
        Get-ChildItem "$env:LOCALAPPDATA\Microsoft\WinGet\Links" -Filter ffmpeg.exe -ErrorAction SilentlyContinue
        Get-ChildItem "$env:LOCALAPPDATA\Microsoft\WinGet\Packages" -Filter ffmpeg.exe -Recurse -ErrorAction SilentlyContinue
    ) | Select-Object -First 1
    if ($found) { $ffmpegCmd = Get-Command $found.FullName }
}
if (-not $ffmpegCmd) {
    Write-Error "ffmpeg was not found. Install it (winget install Gyan.FFmpeg) or pass its path with -Ffmpeg."
    exit 1
}

New-Item -ItemType Directory -Force $MusicDir | Out-Null
$MusicDir = (Resolve-Path $MusicDir).Path

foreach ($pair in @(@($City, "hidef_city.ogg"), @($Combat, "hidef_combat.ogg"))) {
    $src = (Resolve-Path $pair[0]).Path
    $dst = Join-Path $MusicDir $pair[1]
    Write-Host "$src -> $dst"
    & $ffmpegCmd.Source -y -hide_banner -loglevel error -i $src -vn -c:a libvorbis -q:a 6 -ar 44100 -ac 2 $dst
    if ($LASTEXITCODE -ne 0) { Write-Error "Converting $src failed."; exit 1 }
}
Write-Host "Done. In the game, turn on HIDEF MUSIC in the pause menu."
