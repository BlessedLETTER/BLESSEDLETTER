param([int]$x1 = 0, [int]$y1 = 0, [int]$x2 = 0, [int]$y2 = 0, [int]$steps = 6)
# DPI-aware real mouse drag (down at x1,y1 -> move in steps -> up at x2,y2)
Add-Type @"
using System;
using System.Runtime.InteropServices;
public class Md {
  [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
  [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
  [DllImport("user32.dll")] public static extern void mouse_event(uint f, uint dx, uint dy, uint d, IntPtr e);
}
"@
[void][Md]::SetProcessDPIAware()
[void][Md]::SetCursorPos($x1, $y1)
Start-Sleep -Milliseconds 250
[Md]::mouse_event(0x0002, 0, 0, 0, [IntPtr]::Zero)   # down
Start-Sleep -Milliseconds 150
for ($i = 1; $i -le $steps; $i++) {
  $x = [int]($x1 + ($x2 - $x1) * $i / $steps)
  $y = [int]($y1 + ($y2 - $y1) * $i / $steps)
  [void][Md]::SetCursorPos($x, $y)
  Start-Sleep -Milliseconds 90
}
Start-Sleep -Milliseconds 120
[Md]::mouse_event(0x0004, 0, 0, 0, [IntPtr]::Zero)   # up
Write-Output ("dragged " + $x1 + "," + $y1 + " -> " + $x2 + "," + $y2)
