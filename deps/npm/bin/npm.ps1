#!/usr/bin/env pwsh
$basedir=Split-Path $MyInvocation.MyCommand.Definition -Parent

$exe=""
if ($PSVersionTable.PSVersion -lt "6.0" -or $IsWindows) {
  # Fix case when both the Windows and Linux builds of Node
  # are installed in the same directory
  $exe=".exe"
}
$ret=0

$nodeexe = "node$exe"
$nodebin = $(Get-Command $nodeexe -ErrorAction SilentlyContinue -ErrorVariable F).Source
if ($nodebin -eq $null) {
  Write-Host "$nodeexe not found."
  exit 1
}
$nodedir = $(New-Object -ComObject Scripting.FileSystemObject).GetFile("$nodebin").ParentFolder.Path

$npmclijs="$nodedir/node_modules/npm/bin/npm-cli.js"
$npmprefix=(& $nodeexe $npmclijs prefix -g)
if ($LASTEXITCODE -ne 0) {
  Write-Host "Could not determine Node.js install directory"
  exit 1
}
$npmprefixclijs="$npmprefix/node_modules/npm/bin/npm-cli.js"

# Support pipeline input
if ($MyInvocation.ExpectingInput) {
  $input | & $nodeexe $npmprefixclijs $args
} else {
  & $nodeexe $npmprefixclijs $args
}
$ret=$LASTEXITCODE
exit $ret
