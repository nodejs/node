#!python

# Copyright (c) 2015 Ryan Prichard
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to
# deal in the Software without restriction, including without limitation the
# rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
# sell copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
# IN THE SOFTWARE.

#
# Run with native CPython 2.7 on a 64-bit computer.
#
# Each of the targets in BUILD_TARGETS must be installed to the default
# location.  Each target must have the appropriate MinGW and non-MinGW
# compilers installed, as well as make and tar.
#

import common_ship

import multiprocessing
import os
import shutil
import subprocess
import sys

os.chdir(common_ship.topDir)

def dllVersion(path):
    version = subprocess.check_output(
        ["powershell.exe",
        "[System.Diagnostics.FileVersionInfo]::GetVersionInfo(\"" + path + "\").FileVersion"])
    return version.strip()

# Determine other build parameters.
print "Determining Cygwin/MSYS2 DLL versions..."
sys.stdout.flush()
BUILD_TARGETS = [
    # {
    #     "name": "msys",
    #     "path": "C:\\MinGW\\bin;C:\\MinGW\\msys\\1.0\\bin",
    #     # The parallel make.exe in the original MSYS/MinGW project hangs.
    #     "make_binary": "mingw32-make.exe",
    # },
    {
        "name": "msys2-" + dllVersion("C:\\msys32\\usr\\bin\\msys-2.0.dll") + "-ia32",
        "path": "C:\\msys32\\mingw32\\bin;C:\\msys32\\usr\\bin",
    },
    {
        "name": "msys2-" + dllVersion("C:\\msys64\\usr\\bin\\msys-2.0.dll") + "-x64",
        "path": "C:\\msys64\\mingw64\\bin;C:\\msys64\\usr\\bin",
    },
    {
        "name": "cygwin-" + dllVersion("C:\\cygwin\\bin\\cygwin1.dll") + "-ia32",
        "path": "C:\\cygwin\\bin",
    },
    {
        "name": "cygwin-" + dllVersion("C:\\cygwin64\\bin\\cygwin1.dll") + "-x64",
        "path": "C:\\cygwin64\\bin",
    },
]

def buildTarget(target):
    packageName = "winpty-" + common_ship.winptyVersion + "-" + target["name"]
    if os.path.exists("ship\\packages\\" + packageName):
        shutil.rmtree("ship\\packages\\" + packageName)
    oldPath = os.environ["PATH"]
    os.environ["PATH"] = target["path"] + ";" + common_ship.defaultPathEnviron
    subprocess.check_call(["sh.exe", "configure"])
    makeBinary = target.get("make_binary", "make.exe")
    subprocess.check_call([makeBinary, "clean"])
    makeBaseCmd = [
        makeBinary,
        "USE_PCH=0",
        "COMMIT_HASH=" + common_ship.commitHash,
        "PREFIX=ship/packages/" + packageName
    ]
    subprocess.check_call(makeBaseCmd + ["all", "tests", "-j%d" % multiprocessing.cpu_count()])
    subprocess.check_call(["build\\trivial_test.exe"])
    subprocess.check_call(makeBaseCmd + ["install"])
    subprocess.check_call(["tar.exe", "cvfz",
        packageName + ".tar.gz",
        packageName], cwd=os.path.join(os.getcwd(), "ship", "packages"))
    os.environ["PATH"] = oldPath

def main():
    oldPath = os.environ["PATH"]
    for t in BUILD_TARGETS:
        os.environ["PATH"] = t["path"] + ";" + common_ship.defaultPathEnviron
        subprocess.check_output(["tar.exe", "--help"])
        subprocess.check_output(["make.exe", "--help"])
    for t in BUILD_TARGETS:
        buildTarget(t)

if __name__ == "__main__":
    main()
