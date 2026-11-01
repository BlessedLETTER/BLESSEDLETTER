param([long]$hwnd = 0, [int]$ch = 65)
Add-Type @"
using System;
using System.Runtime.InteropServices;
public class Cp {
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
}
"@
$h = [IntPtr]$hwnd
[void][Cp]::PostMessage($h, 0x0100, [IntPtr]$ch, [IntPtr]0x001E0001)
[void][Cp]::PostMessage($h, 0x0102, [IntPtr]$ch, [IntPtr]0x001E0001)
Write-Output "posted keydown+char"
