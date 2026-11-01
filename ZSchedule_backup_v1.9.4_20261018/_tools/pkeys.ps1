param([string]$text = '', [int]$wait = 300)
Add-Type -AssemblyName System.Windows.Forms
Start-Sleep -Milliseconds $wait
[System.Windows.Forms.SendKeys]::SendWait($text)
Write-Output ("sent keys: " + $text)
