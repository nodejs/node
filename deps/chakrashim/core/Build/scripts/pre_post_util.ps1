#-------------------------------------------------------------------------------------------------------
# Copyright (C) Microsoft. All rights reserved.
# Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
#-------------------------------------------------------------------------------------------------------

. "$PSScriptRoot\util.ps1"

function UseValueOrDefault($value, $defaultvalue, $defaultvalue2) {
    if ($value -ne "") {
        return $value;
    } elseif ($defaultvalue -ne "") {
        return $defaultvalue;
    }
    return $defaultvalue2;
}

function WriteCommonArguments() {
    WriteMessage "Source Path  : $srcpath"
    WriteMessage "Object Path  : $objpath"
    WriteMessage "Binaries Path: $binpath"
}

function GetGitPath()
{
    $gitExe = "git.exe"

    if (!(Get-Command $gitExe -ErrorAction SilentlyContinue)) {
        $gitExe = "C:\1image\Git\bin\git.exe"
        if (!(Test-Path $gitExe)) {
            throw "git.exe not found in path- aborting."
        }
    }
    return $gitExe;
}

$srcpath = UseValueOrDefault $srcpath "$env:TF_BUILD_SOURCESDIRECTORY" (Resolve-Path "$OutterScriptRoot\..\..");
$binpath = UseValueOrDefault $binpath "$env:TF_BUILD_BINARIESDIRECTORY" "$srcpath\Build\VcBuild\bin\$arch_$flavor";
$objpath = UseValueOrDefault $objpath "$env:TF_BUILD_BUILDDIRECTORY" "$srcpath\Build\VcBuild\obj\$arch_$flavor";


