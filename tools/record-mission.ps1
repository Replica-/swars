<#
.SYNOPSIS
    Launches a mission and records a video of its window, from the moment the window opens
    to the moment it is closed.

.DESCRIPTION
    Needs ffmpeg (https://ffmpeg.org; "winget install Gyan.FFmpeg"), found on the PATH or given
    with -Ffmpeg. The recording is written to the "videos" folder next to the repository, named
    after the mission and the time. The capture follows the client area of the game window (the
    game has to stay in front, the recording is of the screen).

.EXAMPLE
    ./tools/record-mission.ps1                 # mission 1 of the Eurocorp campaign, the one "Mission: Launch" starts
    ./tools/record-mission.ps1 -Mission "1,2"  # the Epoch mission
#>
param(
    [string]$Mission = "0,1",
    [string]$Exe = "$PSScriptRoot\..\debug\src\syndwarsfx.exe",
    [string]$Ffmpeg = "ffmpeg",
    [int]$Fps = 60,
    [string]$OutDir = "$PSScriptRoot\..\videos"
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

$Exe = (Resolve-Path $Exe).Path
New-Item -ItemType Directory -Force $OutDir | Out-Null
$OutDir = (Resolve-Path $OutDir).Path
$name = "mission-" + ($Mission -replace "[^0-9]+", "-") + "-" + (Get-Date -Format "yyyyMMdd-HHmmss") + ".mp4"
$outFile = Join-Path $OutDir $name

Add-Type @"
using System;
using System.Runtime.InteropServices;
public class RecWin {
    [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
    [DllImport("user32.dll")] public static extern bool IsWindow(IntPtr h);
    [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
    [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref POINT p);
    [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L, T, R, B; }
    [StructLayout(LayoutKind.Sequential)] public struct POINT { public int X, Y; }
}
"@
[RecWin]::SetProcessDPIAware() | Out-Null

Add-Type -Path "$PSScriptRoot\LoopbackRecorder.cs"
$clock = [System.Diagnostics.Stopwatch]::StartNew()

# The game, started as "Mission: Launch" starts it
$game = Start-Process -FilePath $Exe -ArgumentList @("-m", $Mission, "-q", "-W") `
    -WorkingDirectory (Split-Path $Exe) -PassThru

# Waiting for the window of the game
$hwnd = [IntPtr]::Zero
while (-not $game.HasExited) {
    $game.Refresh()
    if ($game.MainWindowHandle -ne [IntPtr]::Zero) { $hwnd = $game.MainWindowHandle; break }
    Start-Sleep -Milliseconds 100
}
if ($hwnd -eq [IntPtr]::Zero) {
    Write-Error "The game ended before it opened its window."
    exit 1
}

# The client area of the window, on the screen; the sizes are even, for the encoder
$rect = New-Object RecWin+RECT
[RecWin]::GetClientRect($hwnd, [ref]$rect) | Out-Null
$pt = New-Object RecWin+POINT
[RecWin]::ClientToScreen($hwnd, [ref]$pt) | Out-Null
$w = $rect.R - ($rect.R % 2)
$h = $rect.B - ($rect.B % 2)

# The recording starts as soon as the window is there: the sound which the computer plays (ffmpeg cannot take it
# itself) is recorded to a wav file, and the picture by ffmpeg; they are put together when the recording ends
$videoFile = [System.IO.Path]::ChangeExtension($outFile, ".video.mp4")
$audioFile = [System.IO.Path]::ChangeExtension($outFile, ".audio.wav")

$audio = New-Object LoopbackRecorder
$audio.Start($audioFile, $clock)
if ($audio.Error) { Write-Warning "No sound will be recorded: $($audio.Error)" }

$psi = New-Object System.Diagnostics.ProcessStartInfo
$psi.FileName = $ffmpegCmd.Source
$psi.Arguments = "-y -f gdigrab -framerate $Fps -offset_x $($pt.X) -offset_y $($pt.Y) -video_size ${w}x${h} -draw_mouse 0 -i desktop " +
    "-c:v libx264 -preset ultrafast -crf 18 -pix_fmt yuv420p `"$videoFile`""
$psi.UseShellExecute = $false
$psi.RedirectStandardInput = $true
$psi.RedirectStandardError = $true
$psi.CreateNoWindow = $true
$rec = [System.Diagnostics.Process]::Start($psi)
$watch = New-Object FfmpegWatch
$watch.Attach($rec, $clock)
Write-Host "Recording to $outFile"

# ... and stops as soon as the window is closed
while (-not $game.HasExited -and [RecWin]::IsWindow($hwnd)) {
    if ($rec.HasExited) { break }
    Start-Sleep -Milliseconds 50
}

$audio.Stop()
if (-not $rec.HasExited) {
    # "q" makes ffmpeg finish the file properly
    $rec.StandardInput.WriteLine("q")
    $rec.StandardInput.Close()
    if (-not $rec.WaitForExit(15000)) { $rec.Kill() }
}
if (-not $game.HasExited) { $game.WaitForExit(5000) | Out-Null }

# The sound and the picture in one file; the sound is started a little before the picture, which is cut off
if ((Test-Path $audioFile) -and (-not $audio.Error) -and (Test-Path $videoFile)) {
    $offset = 0.0
    if ($watch.VideoStartMs -ge 0) { $offset = [Math]::Max(0.0, ($watch.VideoStartMs - $audio.StartMs) / 1000.0) }
    $offsetArg = $offset.ToString([System.Globalization.CultureInfo]::InvariantCulture)
    & $ffmpegCmd.Source -y -hide_banner -loglevel error -i $videoFile -ss $offsetArg -i $audioFile `
        -map 0:v -map 1:a -c:v copy -c:a aac -ac 2 -b:a 192k -shortest $outFile
    if ($LASTEXITCODE -eq 0) {
        Remove-Item $videoFile, $audioFile -ErrorAction SilentlyContinue
    } else {
        Write-Warning "Putting the sound to the picture failed; the parts are kept: $videoFile, $audioFile"
    }
} elseif (Test-Path $videoFile) {
    Move-Item -Force $videoFile $outFile
    Remove-Item $audioFile -ErrorAction SilentlyContinue
}

Write-Host "Saved $outFile"