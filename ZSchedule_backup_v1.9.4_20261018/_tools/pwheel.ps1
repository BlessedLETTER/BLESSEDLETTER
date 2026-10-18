param([int]$x = 0, [int]$y = 0, [int]$delta = 120)
Add-Type @"
using System;
using System.Runtime.InteropServices;
public class Mw {
  [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
  [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
  [DllImport("user32.dll")] public static extern void mouse_event(uint f, uint dx, uint dy, uint d, IntPtr e);
}
"@
[void][Mw]::SetProcessDPIAware()
[void][Mw]::SetCursorPos($x, $y)
Start-Sleep -Milliseconds 250
$u = [BitConverter]::ToUInt32([BitConverter]::GetBytes([int32]$delta), 0)
[Mw]::mouse_event(0x0800, 0, 0, $u, [IntPtr]::Zero)
Write-Output ("wheel " + $delta + " at " + $x + "," + $y)
