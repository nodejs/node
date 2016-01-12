#-------------------------------------------------------------------------------------------------------
# Copyright (C) Microsoft. All rights reserved.
# Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
#-------------------------------------------------------------------------------------------------------

param (
    [ValidateSet("x86", "x64", "arm", "*")]
    [string]$arch="*",

    [ValidateSet("debug", "release", "test", "codecoverage", "*")]
    [string]$flavor = "*",

    [string]$srcpath = "",
    [string]$binpath = "",
    [string]$objpath = "",

    [string]$srcsrvcmdpath = "Build\script\srcsrv.bat",
    [string]$bvtcmdpath="",
    [string]$repo = "core",
    [string]$logFile = "",

    # comma separated list of [arch,flavor,arch2,flavor2,...] to build
    [string[]]$pogo = @(),
    [string]$pogoscript="",

    [switch]$noaction
)

$global:exitcode = 0

if ($arch -eq "*") {

    . "$PSScriptRoot\util.ps1"
    foreach ($arch in ("x86", "x64", "arm")) {
        ExecuteCommand "$PSScriptRoot\post_build.ps1 -arch $arch -flavor $flavor -srcpath ""$srcpath"" -binpath ""$binpath"" -objpath ""$objpath"" -srcsrvcmdpath ""$srcsrvcmdpath"" -bvtcmdpath ""$bvtcmdpath"" -repo ""$repo""" -logFile ""$logFile"";
    }

} elseif ($flavor -eq "*") {

    . "$PSScriptRoot\util.ps1"
    foreach ($flavor in ("debug", "test", "release")) {
        ExecuteCommand "$PSScriptRoot\post_build.ps1 -arch $arch -flavor $flavor -srcpath ""$srcpath"" -binpath ""$binpath"" -objpath ""$objpath"" -srcsrvcmdpath ""$srcsrvcmdpath"" -bvtcmdpath ""$bvtcmdpath"" -repo ""$repo""" -logFile ""$logFile"";
    }

} else {
    $OutterScriptRoot = $PSScriptRoot;
    . "$PSScriptRoot\pre_post_util.ps1"

    if (($logFile -eq "") -and (Test-Path Env:\TF_BUILD_BINARIESDIRECTORY)) {
        $logFile = "$Env:TF_BUILD_BINARIESDIRECTORY\logs\post_build_$arch_$flavor.log"
        if (Test-Path -Path $logFile) {
            Remove-Item $logFile -Force
        }
    }


    WriteMessage "======================================================================================"
    WriteMessage "Post build script for $arch $flavor";
    WriteMessage "======================================================================================"
    $bvtcmdpath =  UseValueOrDefault $bvtcmdpath "" (Resolve-Path "$PSScriptRoot\..\..\test\runcitests.cmd");

    WriteCommonArguments;
    WriteMessage "BVT Command  : $bvtcmdpath"
    WriteMessage ""

    $srcsrvcmd = ("{0} {1} {2} {3}\bin\{4}_{5}\*.pdb" -f $srcsrvcmdpath, $repo, $srcpath, $binpath, $arch, $flavor);
    $pogocmd = ("{0} {1} {2}" -f $pogoscript, $arch, $flavor);
    $prefastlog = ("{0}\logs\PrefastCheck_{1}_{2}.log" -f $binpath, $arch, $flavor);
    $prefastcmd = "$PSScriptRoot\check_prefast_error.ps1 -directory $objpath -logFile $prefastlog";

    # generate srcsrv
    if ((Test-Path $srcsrvcmdpath) -and (Test-Path $srcpath) -and (Test-Path $binpath)) {
        ExecuteCommand($srcsrvcmd);
    }

    # do PoGO
    $doPogo=$False
    for ($i=0; $i -lt $pogo.length; $i=$i+2) {
        if (($pogo[$i] -eq $arch) -and ($pogo[$i+1] -eq $flavor)) {
            $doPogo=$True
        }
    }
    if ($doPogo) {
        WriteMessage "Building pogo for $arch $flavor"
        ExecuteCommand($pogocmd);
    }

    # run test
    ExecuteCommand("$bvtcmdpath -$arch$flavor");

    # check prefast
    ExecuteCommand($prefastcmd);

    WriteMessage "";
}

exit $global:exitcode

