#-------------------------------------------------------------------------------------------------------
# Copyright (C) Microsoft. All rights reserved.
# Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
#-------------------------------------------------------------------------------------------------------

#
# Pre-Build script
#
# This script is fairly simple. It checks if it's running
# in a VSO. If it is, it uses the VSO environment variables
# to figure out the commit that triggered the build, and if
# such a commit exists, it saves it's description to the build
# output to make it easy to inspect builds.
#
# Require Environment:
#   $TF_BUILD_SOURCEGETVERSION
#   $TF_BUILD_DROPLOCATION
#
# Inferable Environment:
#   $TF_BUILD_SOURCESDIRECTORY
#   $TF_BUILD_BUILDBINARIESDIRECTORY
#   $TF_BUILD_BUILDDIRECTORY
#
# Optional information:
#   $TF_BUILD_BUILDDEFINITIONNAME
#   $TF_BUILD_BUILDNUMBER
#   $TF_BUILD_BUILDURI

param (
    [string]$srcpath = "",
    [string]$binpath = "",
    [string]$objpath = "",
    [string]$logFile = "",
    [string]$oauth
)

$OutterScriptRoot = $PSScriptRoot;
. "$PSScriptRoot\pre_post_util.ps1"
if (($logFile -eq "") -and (Test-Path Env:\TF_BUILD_BINARIESDIRECTORY)) {
    if (-not(Test-Path -Path "$Env:TF_BUILD_BINARIESDIRECTORY\logs")) {
        $dummy = New-Item -Path "$Env:TF_BUILD_BINARIESDIRECTORY\logs" -ItemType Directory -Force
    }
    $logFile = "$Env:TF_BUILD_BINARIESDIRECTORY\logs\pre_build.log"
    if (Test-Path -Path $logFile) {
        Remove-Item $logFile -Force
    }
}

WriteCommonArguments;

if (Test-Path Env:\TF_BUILD_SOURCEGETVERSION)
{
    $commitHash = ($Env:TF_BUILD_SOURCEGETVERSION).split(':')[2]
    $gitExe = GetGitPath;

    $outputDir = $Env:TF_BUILD_DROPLOCATION
    if (-not(Test-Path -Path $outputDir)) {
        $dummy = New-Item -Path $outputDir -ItemType Directory -Force
    }

    Push-Location $srcpath;
    $outputFile = Join-Path -Path $outputDir -ChildPath "change.txt"

    Write-Output "TF_BUILD_BUILDDEFINITIONNAME = $Env:TF_BUILD_BUILDDEFINITIONNAME" | Out-File $outputFile
    Write-Output "TF_BUILD_BUILDNUMBER = $Env:TF_BUILD_BUILDNUMBER" | Out-File $outputFile -Append
    Write-Output "TF_BUILD_SOURCEGETVERSION = $Env:TF_BUILD_SOURCEGETVERSION" | Out-File $outputFile -Append
    Write-Output "TF_BUILD_BUILDURI = $Env:TF_BUILD_BUILDURI" | Out-File $outputFile -Append
    Write-Output "" | Out-File $outputFile -Append


    # Get the git remote path and construct the rest API URI
    $remote = (iex "$gitExe remote -v")[0].split()[1].replace("_git", "_apis/git/repositories");
    $remote = $remote.replace("mshttps", "https");

    # Get the pushId and push date time to use that for build number and build date time
    $uri = ("{0}/commits/{1}?api-version=1.0" -f $remote, $commitHash)
    $oauthToken = Get-Content $oauth;
    $header = @{Authorization=("Basic {0}" -f $oauthToken) }
    $info = Invoke-RestMethod -Headers $header -Uri $uri -Method GET

    $buildDate = ([datetime]$info.push.date).toString("yyMMdd-HHmm")
    $buildPushId = $info.push.pushId;
    $buildPushIdPart1 = [int]($buildPushId / 65536);
    $buildPushIdPart2 = [int]($buildPushId % 65536);
    Write-Output ("PushId = $buildPushId ({0}.{1})" -f $buildPushIdPart1.ToString("00000"), $buildPushIdPart2.ToString("00000")) | Out-File $outputFile -Append
    Write-Output "PushDate = $buildDate" | Out-File $outputFile -Append
    Write-Output "" | Out-File $outputFile -Append

    # commit message
    $command = "$gitExe log -1 --name-status -p $commitHash"
    iex $command | Out-File $outputFile -Append
    Pop-Location


    # commit hash
    $buildCommit = ($Env:TF_BUILD_SOURCEGETVERSION).SubString(14);

    $buildInfoOutputDir = $objpath
    if (-not(Test-Path -Path $buildInfoOutputDir)) {
        $dummy = New-Item -Path $buildInfoOutputDir -ItemType Directory -Force
    }

    # generate build version prop file
    $buildInfoOutputFile = Join-Path -Path $buildInfoOutputDir -ChildPath "Chakra.Generated.BuildInfo.props"
    $propsFile = @"
<?xml version="1.0" encoding="utf-16"?>
<Project DefaultTargets="Build" ToolsVersion="4.0" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <PropertyGroup>
    <OutBaseDir>{0}</OutBaseDir>
    <IntBaseDir>{1}</IntBaseDir>
    <ChakraVersionBuildNumber>{2}</ChakraVersionBuildNumber>
    <ChakraVersionBuildQFENumber>{3}</ChakraVersionBuildQFENumber>
    <ChakraVersionBuildCommit>{4}</ChakraVersionBuildCommit>
    <ChakraVersionBuildDate>{5}</ChakraVersionBuildDate>
  </PropertyGroup>
</Project>
"@
    Write-Output ($propsFile -f $binpath, $objpath, $buildPushIdPart1, $buildPushIdPart2, $buildCommit, $buildDate) | Out-File $buildInfoOutputFile
}

# Clean up code analysis summary files in case they get left behind
if (Test-Path $objpath) {
    Get-ChildItem $objpath -include vc.nativecodeanalysis.all.xml -recurse | Remove-Item
}
