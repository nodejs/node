# Copyright 2017 - Refael Ackermann
# Distributed under MIT style license
# See accompanying file LICENSE at https://github.com/node4good/windows-autoconf
# version: 1.12.2

param (
    [Parameter(Mandatory=$false)]
    [string[]]$filters = "IsVcCompatible",
    [Parameter(Mandatory=$false)]
    [string[]]$keys = "VisualCppToolsCL",
    [Parameter(Mandatory=$false)]
    [switch]$Information
)
$invocation = (Get-Variable MyInvocation).Value
cd (Split-Path $invocation.MyCommand.Path)
if (-NOT (Test-Path 'Registry::HKEY_CLASSES_ROOT\CLSID\{177F0C4A-1CD3-4DE7-A32C-71DBBB9FA36D}')) { Exit 1 }
Add-Type -Path GetVS2017Configuration.cs;
$insts = [VisualStudioConfiguration.ComSurrogate]::QueryEx()
if ($Information) {
    $insts | % { [Console]::Error.WriteLine('Found VS2017 "' +$_['Name'] + '" installed at "' + $_['InstallationPath'] + '"') }
}
$insts | % { $_.Add('Fail', @()) }
foreach ($filter in $filters) {
    if ($filter -eq "*") { continue; }
    if ($filter -like "*=*") {
         $parts = $filter -Split "=";
         $filter = $parts[0];
         $criteria = $parts[1];
         $insts | % { if ($_.Get($filter) -ne $criteria) { $_['Fail'] += $filter } }
     } else {
         $insts | % { if (!($_.Get($filter))) { $_['Fail'] += $filter } }
     }
}
if ($Information) {
    $insts | % { if ($_['Fail'].length -ne 0) {[Console]::Error.WriteLine('"' +$_['Name'] + '" installed in "' + $_['InstallationPath'] + '" does not meet criteria "' + $_['Fail'] + '"') } }
}
$insts = $insts | where { $_['Fail'].length -eq 0 }
foreach ($key in $keys) {
    if ($key -eq "*") { $insts | echo } else { $insts | % { $_.Get($key) } }
}
