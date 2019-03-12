Set-Location $PSScriptRoot;
Import-Module .\Microsoft.VisualStudio.Setup.PowerShell.dll
Get-VSSetupInstance -All | Sort-Object @{Expression={$_.InstallationVersion}; Descending=$true} | ConvertTo-Json

