param([long]$hwnd = 0, [int]$ch = 65)
Add-Type @"
using System;
using System.Runtime.InteropServices;
public class Cc {
  [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern int GetWindowLong(IntPtr h, int i);
}
"@
$h = [IntPtr]$hwnd
$style = [Cc]::GetWindowLong($h, -16)
Write-Output ("style=0x" + $style.ToString("X") + " readonly=" + (($style -band 0x800) -ne 0))
[void][Cc]::SendMessage($h, 0x0102, [IntPtr]$ch, [IntPtr]1)   # WM_CHAR
Write-Output "sent WM_CHAR"
