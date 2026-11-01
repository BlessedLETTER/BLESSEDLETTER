param([int]$procId = 0, [string]$cls = "ZScheduleMainWnd", [int]$id = 1016)
Add-Type @"
using System;
using System.Runtime.InteropServices;
public class Cr {
  public delegate bool EnumProc(IntPtr h, IntPtr l);
  [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr l);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassName(IntPtr h, System.Text.StringBuilder s, int n);
  [DllImport("user32.dll")] public static extern IntPtr GetDlgItem(IntPtr h, int id);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int l, t, r, b; }
}
"@
[void][Cr]::SetProcessDPIAware()
$script:target = [IntPtr]::Zero
$cb = [Cr+EnumProc]{ param($h,$l)
  $p = 0; [void][Cr]::GetWindowThreadProcessId($h, [ref]$p)
  $sb = New-Object System.Text.StringBuilder 256
  [void][Cr]::GetClassName($h, $sb, 256)
  if ($p -eq $procId -and $sb.ToString() -eq $cls) { $script:target = $h; return $false }
  return $true
}
[void][Cr]::EnumWindows($cb, [IntPtr]::Zero)
if ($script:target -eq [IntPtr]::Zero) { Write-Output "main not found"; exit 1 }
$c = [Cr]::GetDlgItem($script:target, $id)
$r = New-Object Cr+RECT
[void][Cr]::GetWindowRect($c, [ref]$r)
Write-Output ("ctrl " + $id + " hwnd=" + $c + " rect=" + $r.l + "," + $r.t + "," + $r.r + "," + $r.b)
