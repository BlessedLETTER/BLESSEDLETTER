param([string]$text = "abc")
Add-Type @"
using System;
using System.Runtime.InteropServices;
public class Tp {
  [DllImport("user32.dll")] public static extern void keybd_event(byte vk, byte scan, uint flags, IntPtr extra);
  [DllImport("user32.dll")] public static extern short VkKeyScan(char ch);
}
"@
foreach ($c in $text.ToCharArray()) {
  $v = [Tp]::VkKeyScan($c)
  $vk = [byte]($v -band 0xff)
  [Tp]::keybd_event($vk, 0, 0, [IntPtr]::Zero)
  Start-Sleep -Milliseconds 40
  [Tp]::keybd_event($vk, 0, 2, [IntPtr]::Zero)
  Start-Sleep -Milliseconds 40
}
Write-Output ("typed " + $text)
