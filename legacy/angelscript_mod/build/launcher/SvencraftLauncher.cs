// Svencraft Coop launcher: Steam's "Svencraft Coop" shortcut runs this from <game>\launcher\ (a folder with
// no steam_appid.txt), so Steam tracks the shortcut rather than Sven Co-op. It starts the real game from the
// parent folder with the same arguments and exits when the game exits.
using System;
using System.Diagnostics;
using System.IO;
using System.Text;

class SvencraftLauncher
{
    static string Quote(string a)
    {
        return (a.Length > 0 && a.IndexOfAny(new[] { ' ', '\t', '"' }) < 0) ? a : "\"" + a.Replace("\"", "\\\"") + "\"";
    }

    static int Main(string[] args)
    {
        string gameDir = Path.GetFullPath(Path.Combine(AppDomain.CurrentDomain.BaseDirectory, ".."));
        var cmd = new StringBuilder();
        foreach (string a in args)
            cmd.Append(Quote(a)).Append(' ');
        var psi = new ProcessStartInfo(Path.Combine(gameDir, "svencoop.exe"), cmd.ToString().TrimEnd())
        {
            WorkingDirectory = gameDir,
            UseShellExecute = false
        };
        using (Process game = Process.Start(psi))
        {
            game.WaitForExit();
            return game.ExitCode;
        }
    }
}
