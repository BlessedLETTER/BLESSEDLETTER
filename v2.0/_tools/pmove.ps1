param([int]$pid_ = 0, [string]$cls = "ZScheduleMainWnd", [int]$x = 0, [int]$y = 0)
Add-Type @"
using System;
using System.Runtime.InteropServices;
public class Mv {
  public delegate bool EnumProc(IntPtr h, IntPtr l);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr l);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassName(IntPtr h, System.Text.StringBuilder s, int n);
  [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h, IntPtr after, int x, int y, int cx, int cy, uint flags);
}
"@
$target = [IntPtr]::Zero
$cb = [Mv+EnumProc]{ param($h,$l)
  $p = 0; [void][Mv]::GetWindowThreadProcessId($h, [ref]$p)
  $sb = New-Object System.Text.StringBuilder 256
  [void][Mv]::GetClassName($h, $sb, 256)
  if ($p -eq $pid_ -and $sb.ToString() -eq $cls -and [Mv]::IsWindowVisible($h)) { $script:target = $h; return $false }
  return $true
}
[void][Mv]::SetProcessDPIAware()
[void][Mv]::EnumWindows($cb, [IntPtr]::Zero)
if ($target -ne [IntPtr]::Zero) {
  [void][Mv]::SetWindowPos($target, [IntPtr]::Zero, $x, $y, 0, 0, 0x0001 -bor 0x0004)
  Write-Output ("moved " + $cls + " to " + $x + "," + $y)
} else { Write-Output "not found" }
