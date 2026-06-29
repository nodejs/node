The Microsoft Visual Studio project files for including V8 in a Visual
Studio/Visual C++ Express solution has been retired. If a Visual
Studio project/solution is needed there is the option of using GYP to
generate these. Please look in the build directory in the root of the
V8 project. It contains the required infrastructure and a README.txt
file explaining how to get started.

Generating Visual Studio projects using GYP is how the Chromium
project integrated V8 into the Windows build.

The main build system for V8 is still SCons, see the V8 wiki page
http://code.google.com/p/v8/wiki/BuildingOnWindows for details.
