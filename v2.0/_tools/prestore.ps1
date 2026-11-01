param([int]$procId = 0, [string]$cls = "ZScheduleMainWnd")
Add-Type @"
using System;
using System.Runtime.InteropServices;
public class Rw {
  public delegate bool EnumProc(IntPtr h, IntPtr l);
  [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr l);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassName(IntPtr h, System.Text.StringBuilder s, int n);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int cmd);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
}
"@
[void][Rw]::SetProcessDPIAware()
$script:target = [IntPtr]::Zero
$cb = [Rw+EnumProc]{ param($h,$l)
  $p = 0; [void][Rw]::GetWindowThreadProcessId($h, [ref]$p)
  $sb = New-Object System.Text.StringBuilder 256
  [void][Rw]::GetClassName($h, $sb, 256)
  if ($p -eq $procId -and $sb.ToString() -eq $cls) { $script:target = $h; return $false }
  return $true
}
[void][Rw]::EnumWindows($cb, [IntPtr]::Zero)
if ($script:target -eq [IntPtr]::Zero) { Write-Output "not found"; exit 1 }
[void][Rw]::ShowWindow($script:target, 9)
Start-Sleep -Milliseconds 300
[void][Rw]::SetForegroundWindow($script:target)
Write-Output "restored"
