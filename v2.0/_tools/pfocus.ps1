Add-Type @"
using System;
using System.Runtime.InteropServices;
public class Fc {
  [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
  [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll")] public static extern bool GetGUIThreadInfo(uint tid, ref GUITHREADINFO gti);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassName(IntPtr h, System.Text.StringBuilder s, int n);
  [DllImport("user32.dll")] public static extern bool IsWindowEnabled(IntPtr h);
  [StructLayout(LayoutKind.Sequential)] public struct GUITHREADINFO {
    public int cbSize; public int flags; public IntPtr hwndActive, hwndFocus, hwndCapture,
    hwndMenuOwner, hwndMoveSize, hwndCaret; public RECT rcCaret;
  }
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int l,t,r,b; }
}
"@
[void][Fc]::SetProcessDPIAware()
$fg = [Fc]::GetForegroundWindow()
$p = 0; $tid = [Fc]::GetWindowThreadProcessId($fg, [ref]$p)
$g = New-Object Fc+GUITHREADINFO; $g.cbSize = [Runtime.InteropServices.Marshal]::SizeOf($g)
[void][Fc]::GetGUIThreadInfo($tid, [ref]$g)
$sb1 = New-Object System.Text.StringBuilder 256; [void][Fc]::GetClassName($fg, $sb1, 256)
$sb2 = New-Object System.Text.StringBuilder 256; [void][Fc]::GetClassName($g.hwndFocus, $sb2, 256)
Write-Output ("foreground=" + $sb1.ToString() + " pid=" + $p + " focus=" + $sb2.ToString() + " hwnd=" + $g.hwndFocus + " enabled=" + [Fc]::IsWindowEnabled($g.hwndFocus))
