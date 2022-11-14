param (
    [Parameter(Mandatory=$true)] $CustomActionsProjectFile,
    [Parameter(Mandatory=$true)] $MsbSdk,
    [Parameter(Mandatory=$true)] $PlatformToolset,
    [Parameter(Mandatory=$true)] $Platform,
    [Parameter(Mandatory=$true)] $NpmDirectory,
    [Parameter(Mandatory=$true)] $NpmWxs,
    [Parameter(Mandatory=$true)] $CorepackDirectory,
    [Parameter(Mandatory=$true)] $CorepackWxs,
    [Parameter(Mandatory=$true)] $ProductVersion,
    [Parameter(Mandatory=$true)] $FullVersion,
    [Parameter(Mandatory=$true)] $DistTypeDir,
    [Parameter(Mandatory=$true)] $ProjectDir,
    [Parameter(Mandatory=$true)] $Configuration,
    [Parameter(Mandatory=$true)] $CustomActionsTargetDir,
    [Parameter(Mandatory=$true)] $ProductWxs,
    [Parameter(Mandatory=$true)] $EnUsWxl,
    [Parameter(Mandatory=$true)] $NodeMsi
)

# Directory and file parameters
$RootDirectory = "$PSScriptRoot/wix"

$SdkZip = "$RootDirectory/WixToolset.Sdk.zip"
$HeatZip = "$RootDirectory/WixToolset.Heat.zip"
$WcaUtilZip = "$RootDirectory/WixToolset.WcaUtil.zip"
$DUtilZip = "$RootDirectory/WixToolset.DUtil.zip"
$UtilExtZip = "$RootDirectory/WixToolset.Util.wixext.zip"
$UiExtZip = "$RootDirectory/WixToolset.UI.wixext.zip"

$SdkExport = "$RootDirectory/sdk"
$HeatExport = "$RootDirectory/heat"
$WcaAndDUtilExport = "$RootDirectory/utils"
$UtilExtExport = "$RootDirectory/util"
$UiExtExport = "$RootDirectory/ui"

$WixFile = "$SdkExport/tools/net472/x64/wix.exe"
$HeatFile = "$HeatExport/tools/net472/x64/heat.exe"
$WcaAndDUtilDirectory = "$WcaAndDUtilExport/build/native"
$UtilExtFile = "$UtilExtExport/wixext4/WixToolset.Util.wixext.dll"
$UiExtFile = "$UiExtExport/wixext4/WixToolset.UI.wixext.dll"

# WiX NuGet package parameters
$wixVersion = "4.0.0-preview.1"
$SdkUri = "https://www.nuget.org/api/v2/package/WixToolset.Sdk/$wixVersion"
$HeatUri = "https://www.nuget.org/api/v2/package/WixToolset.Heat/$wixVersion"
$WcaUtilUri = "https://www.nuget.org/api/v2/package/WixToolset.WcaUtil/$wixVersion"
$DUtilUri = "https://www.nuget.org/api/v2/package/WixToolset.DUtil/$wixVersion"
$UtilUri = "https://www.nuget.org/api/v2/package/WixToolset.Util.wixext/$wixVersion"
$UiUri = "https://www.nuget.org/api/v2/package/WixToolset.UI.wixext/$wixVersion"

# Create clean root directories
if (Get-Item $RootDirectory) {
    Remove-Item $RootDirectory -Recurse
}
New-Item $RootDirectory -ItemType Directory

# Download required WiX NuGet packages
Invoke-WebRequest -Uri $SdkUri -OutFile $SdkZip
Invoke-WebRequest -Uri $HeatUri -OutFile $heatZip
Invoke-WebRequest -Uri $WcaUtilUri -OutFile $WcaUtilZip
Invoke-WebRequest -Uri $DUtilUri -OutFile $DUtilZip
Invoke-WebRequest -Uri $UtilUri -OutFile $UtilExtZip
Invoke-WebRequest -Uri $UiUri -OutFile $UiExtZip

# Export downloaded WiX NuGet packages
Expand-Archive -Path $SdkZip -DestinationPath $SdkExport
Expand-Archive -Path $HeatZip -DestinationPath $HeatExport
Expand-Archive -Path $WcaUtilZip -DestinationPath $WcaAndDUtilExport
Expand-Archive -Path $DUtilZip -DestinationPath $WcaAndDUtilExport
Expand-Archive -Path $UtilExtZip -DestinationPath $UtilExtExport
Expand-Archive -Path $UiExtZip -DestinationPath $UiExtExport

# Build node custom actions project
$process = Start-Process -FilePath "msbuild" -ArgumentList "$CustomActionsProjectFile /m /t:Clean,Build $MsbSdk /p:PlatformToolset=$PlatformToolset /p:WixSdkDir=`"$WcaAndDUtilDirectory`" /p:Configuration=$Configuration /p:Platform=$Platform /p:NodeVersion=$ProductVersion /p:FullVersion=$FullVersion /p:DistTypeDir=$DistTypeDir /clp:NoSummary;NoItemAndPropertyList;Verbosity=minimal /nologo" -PassThru
$process.WaitForExit()

# Run harvest tool (heat.exe) for npm and corepack
$process = Start-Process -FilePath $HeatFile -ArgumentList "dir $NpmDirectory -var var.NpmSourceDir -cg NpmSourceFiles -dr NodeModulesFolder -gg -out $NpmWxs" -PassThru 
$process.WaitForExit()
$process = Start-Process -FilePath $HeatFile -ArgumentList "dir $CorepackDirectory -var var.CorepackSourceDir -cg CorepackSourceFiles -dr NodeModulesFolder -gg -out $CorepackWxs" -PassThru 
$process.WaitForExit()

# Run wix tool (wix.exe) for building the installer
$process = Start-Process -FilePath $WixFile -ArgumentList "build -ext $UtilExtFile -ext $UiExtFile -d ProductVersion=$ProductVersion -d FullVersion=$FullVersion -d DistTypeDir=$DistTypeDir -d NpmSourceDir=$NpmDirectory\ -d CorepackSourceDir=$CorepackDirectory\ -d ProgramFilesFolderId=ProgramFiles64Folder -d ProjectDir=$ProjectDir -d Configuration=$Configuration -d custom_actions.TargetDir=$CustomActionsTargetDir -d custom_actions.TargetName=custom_actions $ProductWxs $NpmWxs $CorepackWxs $EnUsWxl -o $NodeMsi" -PassThru 
$process.WaitForExit()

# Delete everything
Remove-Item $RootDirectory -Recurse