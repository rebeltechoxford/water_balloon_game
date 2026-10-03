using System;
using System.Diagnostics;
using System.IO;
using System.Windows.Forms;

internal static class Program
{
    private static string ResolveExecutable(string baseDirectory, string workingDirectory)
    {
        // F5 emits the launcher into WindowsDemo; also support launching it from
        // the project folder without depending on the shortcut's working folder.
        string parent = Path.GetDirectoryName(baseDirectory.TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar));
        string[] roots = { baseDirectory, parent, workingDirectory };
        foreach (string root in roots)
        {
            if (String.IsNullOrEmpty(root)) continue;
            string unreal = Path.Combine(root, "PackagedUnreal", "Windows", "Clanka.exe");
            if (File.Exists(unreal)) return unreal;
        }
        foreach (string root in roots)
        {
            if (String.IsNullOrEmpty(root)) continue;
            string preview = Path.Combine(root, "ClankaSplashLab.exe");
            if (File.Exists(preview)) return preview;
            preview = Path.Combine(root, "WindowsDemo", "ClankaSplashLab.exe");
            if (File.Exists(preview)) return preview;
        }
        throw new FileNotFoundException("The packaged Unreal demo and native preview are missing. Package Unreal or build the solution first.");
    }

    [STAThread]
    private static int Main(string[] args)
    {
        string directory = AppDomain.CurrentDomain.BaseDirectory;
        try
        {
            string executable = ResolveExecutable(directory, Environment.CurrentDirectory);
            var start = new ProcessStartInfo(executable) { WorkingDirectory = Path.GetDirectoryName(executable), UseShellExecute = false };
            if (args.Length == 1 && args[0] == "--demo-test")
                start.Arguments = Path.GetFileName(executable).Equals("Clanka.exe", StringComparison.OrdinalIgnoreCase) ? "-DemoTest" : "--demo-test";
            using (Process process = Process.Start(start)) { process.WaitForExit(); return process.ExitCode; }
        }
        catch (Exception ex)
        {
            MessageBox.Show(ex.Message, "Clanka Splash Lab", MessageBoxButtons.OK, MessageBoxIcon.Error);
            return 1;
        }
    }
}
