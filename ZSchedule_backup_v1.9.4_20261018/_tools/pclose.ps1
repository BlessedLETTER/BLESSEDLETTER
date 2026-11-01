param([string]$title = '')
# Close every top-level window whose title contains $title (test cleanup helper).
Add-Type @"
using System;
using System.Text;
using System.Collections.Generic;
using System.Runtime.InteropServices;
public class Wz {
  public delegate bool EnumProc(IntPtr h, IntPtr l);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr l);
  [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern int GetWindowText(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  public static List<IntPtr> Find(string t) {
    var r = new List<IntPtr>();
    EnumWindows((h, l) => {
      var s = new StringBuilder(512);
      GetWindowText(h, s, 512);
      if (t.Length > 0 && s.ToString().Contains(t)) r.Add(h);
      return true;
    }, IntPtr.Zero);
    return r;
  }
}
"@
$n = 0
foreach ($h in [Wz]::Find($title)) {
  [void][Wz]::PostMessage($h, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero)
  $n++
}
Write-Output ("closed " + $n)
