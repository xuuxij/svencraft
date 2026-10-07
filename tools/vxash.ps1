# Visual test: launch Svencraft (Xash3D FWGS) windowed, capture the game window twice a second with
# PrintWindow(PW_RENDERFULLCONTENT) (GDI screen grabs come out black for this GL window), then close it.
#   powershell -File vxash.ps1 <outdir> [seconds] [engine args]
param([string]$Out, [int]$Seconds = 30, [string]$Extra = "+map svencraft_sandbox")
if (Get-Process xash3d -ErrorAction SilentlyContinue) { "game already running - not launching"; exit 1 }
Add-Type -ReferencedAssemblies System.Drawing @"
using System; using System.Drawing; using System.Drawing.Imaging; using System.Runtime.InteropServices;
public class VX {
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref POINT p);
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint flags);
  public delegate bool EnumProc(IntPtr h, IntPtr l);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc f, IntPtr l);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern int GetClassName(IntPtr h, System.Text.StringBuilder s, int n);
  // the game's SDL window (the -dev console is a separate window of the same process)
  public static IntPtr GameWindow(uint pid) {
    IntPtr best = IntPtr.Zero;
    EnumWindows((h, l) => {
      uint p; GetWindowThreadProcessId(h, out p);
      if (p != pid || !IsWindowVisible(h)) return true;
      var sb = new System.Text.StringBuilder(256); GetClassName(h, sb, 256);
      if (sb.ToString().StartsWith("SDL")) { best = h; return false; }
      return true;
    }, IntPtr.Zero);
    return best;
  }
  public struct RECT { public int L, T, R, B; }
  public struct POINT { public int X, Y; }
  public static void Shot(IntPtr h, string path) {
    RECT wr; GetWindowRect(h, out wr); RECT cr; GetClientRect(h, out cr);
    POINT p = new POINT(); ClientToScreen(h, ref p);
    int w = wr.R - wr.L, hh = wr.B - wr.T;
    using (var bmp = new Bitmap(w, hh, PixelFormat.Format32bppArgb)) {
      using (var g = Graphics.FromImage(bmp)) { IntPtr dc = g.GetHdc(); PrintWindow(h, dc, 2); g.ReleaseHdc(dc); }
      var crop = new Rectangle(p.X - wr.L, p.Y - wr.T, cr.R - cr.L, cr.B - cr.T);
      using (var c = bmp.Clone(crop, PixelFormat.Format24bppRgb)) c.Save(path, ImageFormat.Png);
    }
  }
}
"@
$dir = Join-Path (Split-Path $PSScriptRoot -Parent) "run"   # <project>\run (this script lives in tools\)
New-Item -ItemType Directory -Force $Out | Out-Null
Get-ChildItem $Out -Filter *.png | ForEach-Object { [IO.File]::Delete($_.FullName) }
try {
  $p = Start-Process -FilePath "$dir\xash3d.exe" -ArgumentList "-game svencraft -windowed -width 1280 -height 720 -dev 2 -log $Extra" -WorkingDirectory $dir -PassThru
  $t0 = Get-Date
  while (((Get-Date) - $t0).TotalSeconds -lt 60) { $pr = Get-Process -Id $p.Id -ErrorAction SilentlyContinue; if (-not $pr) { "game exited early"; break }; if ([VX]::GameWindow([uint32]$p.Id) -ne [IntPtr]::Zero) { break }; [Threading.Thread]::Sleep(500) }
  [Threading.Thread]::Sleep(2000)
  $pr = Get-Process -Id $p.Id -ErrorAction SilentlyContinue
  if ($pr) {
    $h = [VX]::GameWindow([uint32]$p.Id)
    [VX]::SetForegroundWindow($h) | Out-Null
    for ($i = 1; $i -le $Seconds * 2; $i++) {
      if (-not (Get-Process -Id $p.Id -ErrorAction SilentlyContinue)) { "game exited"; break }
      try { [VX]::Shot($h, ("{0}\f_{1:D3}.png" -f $Out, $i)) } catch { "shot failed: $_" }
      [Threading.Thread]::Sleep(500)
    }
  }
  "frames: $((Get-ChildItem $Out -Filter *.png).Count)"
} finally {
  if ($p) { Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue }
}
