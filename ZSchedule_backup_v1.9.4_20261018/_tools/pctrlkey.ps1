param([string]$key = "C")
Add-Type @"
using System;
using System.Runtime.InteropServices;
public class Kb {
  [DllImport("user32.dll")] public static extern void keybd_event(byte vk, byte scan, uint flags, IntPtr extra);
}
"@
$vk = [byte][char]$key.ToUpper()
[Kb]::keybd_event(0x11, 0, 0, [IntPtr]::Zero)          # Ctrl down
Start-Sleep -Milliseconds 60
[Kb]::keybd_event($vk, 0, 0, [IntPtr]::Zero)
Start-Sleep -Milliseconds 60
[Kb]::keybd_event($vk, 0, 2, [IntPtr]::Zero)           # key up
Start-Sleep -Milliseconds 60
[Kb]::keybd_event(0x11, 0, 2, [IntPtr]::Zero)
Write-Output ("ctrl+" + $key)
