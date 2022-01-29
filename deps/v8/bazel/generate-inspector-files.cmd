REM Copyright 2021 the V8 project authors. All rights reserved.
REM Use of this source code is governed by a BSD-style license that can be
REM found in the LICENSE file.

set BAZEL_OUT=%1

REM Bazel nukes all env vars, and we need the following for gn to work
set DEPOT_TOOLS_WIN_TOOLCHAIN=0
set ProgramFiles(x86)=C:\Program Files (x86)
set windir=C:\Windows

REM Create a default GN output folder
cmd.exe /S /E:ON /V:ON /D /c gn gen out/inspector

REM Generate inspector files
cmd.exe /S /E:ON /V:ON /D /c autoninja -C out/inspector gen/src/inspector/protocol/Forward.h

REM Create directories in bazel output folder
MKDIR -p %BAZEL_OUT%\include\inspector
MKDIR -p %BAZEL_OUT%\src\inspector\protocol

REM Copy generated files to bazel output folder
COPY out\inspector\gen\include\inspector\* %BAZEL_OUT%\include\inspector\
COPY out\inspector\gen\src\inspector\protocol\* %BAZEL_OUT%\src\inspector\protocol\