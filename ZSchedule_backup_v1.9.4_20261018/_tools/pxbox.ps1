param([string]$file, [int]$x0, [int]$y0, [int]$x1, [int]$y1, [string]$mode = 'gray')
Add-Type -AssemblyName System.Drawing
$bmp = [System.Drawing.Bitmap]::FromFile((Resolve-Path $file))
$minx=99999; $miny=99999; $maxx=-1; $maxy=-1; $cnt=0
for ($y=$y0; $y -le $y1; $y++) {
  for ($x=$x0; $x -le $x1; $x++) {
    $c = $bmp.GetPixel($x,$y); $hit=$false
    if ($mode -eq 'gray') { $hit = ([Math]::Abs($c.R-$c.G) -lt 8 -and [Math]::Abs($c.G-$c.B) -lt 8 -and $c.R -gt 150 -and $c.R -lt 215) }
    if ($hit) { $cnt++; if ($x -lt $minx){$minx=$x}; if ($x -gt $maxx){$maxx=$x}; if ($y -lt $miny){$miny=$y}; if ($y -gt $maxy){$maxy=$y} }
  }
}
Write-Output ("mode=" + $mode + " count=" + $cnt + " bbox=" + $minx + "," + $miny + "," + $maxx + "," + $maxy)
$bmp.Dispose()
