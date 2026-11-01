param([int]$x = 0, [int]$y = 0, [string]$act = 'click')
Add-Type @"
using System;
using System.Runtime.InteropServices;
public class Mz {
  [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
  [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
  [DllImport("user32.dll")] public static extern void mouse_event(uint f, uint dx, uint dy, uint d, IntPtr e);
}
"@
[void][Mz]::SetProcessDPIAware()
[void][Mz]::SetCursorPos($x, $y)
Start-Sleep -Milliseconds 250
if ($act -eq 'move') { Write-Output ("moved to " + $x + "," + $y); exit }
if ($act -eq 'rclick') {
  [Mz]::mouse_event(0x0008, 0, 0, 0, [IntPtr]::Zero)
  Start-Sleep -Milliseconds 90
  [Mz]::mouse_event(0x0010, 0, 0, 0, [IntPtr]::Zero)
  Write-Output ("right-clicked " + $x + "," + $y); exit
}
if ($act -eq 'dblclick') {
  [Mz]::mouse_event(0x0002, 0, 0, 0, [IntPtr]::Zero)
  Start-Sleep -Milliseconds 40
  [Mz]::mouse_event(0x0004, 0, 0, 0, [IntPtr]::Zero)
  Start-Sleep -Milliseconds 60
  [Mz]::mouse_event(0x0002, 0, 0, 0, [IntPtr]::Zero)
  Start-Sleep -Milliseconds 40
  [Mz]::mouse_event(0x0004, 0, 0, 0, [IntPtr]::Zero)
  Write-Output ("double-clicked " + $x + "," + $y); exit
}
[Mz]::mouse_event(0x0002, 0, 0, 0, [IntPtr]::Zero)
Start-Sleep -Milliseconds 90
[Mz]::mouse_event(0x0004, 0, 0, 0, [IntPtr]::Zero)
Write-Output ("clicked " + $x + "," + $y)
