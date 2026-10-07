# Visual test: launch Svencraft Coop windowed on the sandbox with the dev tour enabled, record the game
# window at 2 fps, then close the game.   powershell -File vtest.ps1 <outdir> [seconds] [first tour step]
param([string]$Out, [int]$Seconds = 60, [int]$Step = 0)
if (Get-Process svencoop -ErrorAction SilentlyContinue) { "game already running - not launching"; exit 1 }
Add-Type @"
using System; using System.Runtime.InteropServices;
public class VT {
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref POINT p);
  public struct RECT { public int L, T, R, B; }
  public struct POINT { public int X, Y; }
}
"@
$dir = "C:\Program Files (x86)\Steam\steamapps\common\Svencraft Coop"
$flag = "$dir\svencoop\scripts\plugins\store\sc_visualtest"
Set-Content -Path $flag -Value "$Step" -Encoding ascii
New-Item -ItemType Directory -Force $Out | Out-Null
Get-ChildItem $Out -Filter *.png | ForEach-Object { [IO.File]::Delete($_.FullName) }
try {
  $p = Start-Process -FilePath "$dir\svencoop.exe" -ArgumentList "-windowed -w 1280 -h 720 -novid -nojoy +motdfile sc_nomotd.txt +map svencraft_sandbox" -WorkingDirectory $dir -PassThru
  $t0 = Get-Date
  while (((Get-Date) - $t0).TotalSeconds -lt 60) { $pr = Get-Process -Id $p.Id -ErrorAction SilentlyContinue; if ($pr -and $pr.MainWindowTitle -match "Sven") { break }; [Threading.Thread]::Sleep(500) }
  [Threading.Thread]::Sleep(3000)
  $h = (Get-Process -Id $p.Id).MainWindowHandle
  [VT]::SetForegroundWindow($h) | Out-Null
  $r = New-Object VT+RECT; [VT]::GetClientRect($h, [ref]$r) | Out-Null
  $pt = New-Object VT+POINT; [VT]::ClientToScreen($h, [ref]$pt) | Out-Null
  $w = $r.R - $r.L; $hh = $r.B - $r.T
  & ffmpeg -hide_banner -loglevel error -f gdigrab -framerate 2 -offset_x $pt.X -offset_y $pt.Y -video_size "${w}x${hh}" -i desktop -t $Seconds "$Out\f_%03d.png"
  "frames: $((Get-ChildItem $Out -Filter *.png).Count)"
} finally {
  if ($p) { Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue }
  [IO.File]::Delete($flag)
}
