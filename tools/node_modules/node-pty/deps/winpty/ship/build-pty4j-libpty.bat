@echo off

setlocal
cd %~dp0..
set Path=C:\Python27;C:\Program Files\Git\cmd;%Path%

call "%VS140COMNTOOLS%\VsDevCmd.bat" || goto :fail

rmdir /s/q build-libpty 2>NUL
mkdir build-libpty\win
mkdir build-libpty\win\x86
mkdir build-libpty\win\x86_64
mkdir build-libpty\win\xp

rmdir /s/q src\Release  2>NUL
rmdir /s/q src\.vs      2>NUL
del src\*.vcxproj src\*.vcxproj.filters src\*.sln src\*.sdf 2>NUL

call vcbuild.bat --msvc-platform Win32 --gyp-msvs-version 2015 --toolset v140_xp || goto :fail
copy src\Release\Win32\winpty.dll           build-libpty\win\xp || goto :fail
copy src\Release\Win32\winpty-agent.exe     build-libpty\win\xp || goto :fail

call vcbuild.bat --msvc-platform Win32 --gyp-msvs-version 2015 || goto :fail
copy src\Release\Win32\winpty.dll           build-libpty\win\x86 || goto :fail
copy src\Release\Win32\winpty-agent.exe     build-libpty\win\x86 || goto :fail

call vcbuild.bat --msvc-platform x64 --gyp-msvs-version 2015 || goto :fail
copy src\Release\x64\winpty.dll             build-libpty\win\x86_64 || goto :fail
copy src\Release\x64\winpty-agent.exe       build-libpty\win\x86_64 || goto :fail

echo success
goto :EOF

:fail
echo error: build failed
exit /b 1
