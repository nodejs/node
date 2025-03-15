@IF NOT DEFINED DEBUG_HELPER @ECHO OFF

ECHO Looking for NASM

FOR /F "delims=" %%a IN ('where nasm 2^> NUL') DO (
  EXIT /B 0
)

IF EXIST "%ProgramFiles%\NASM\nasm.exe" (
  SET "Path=%Path%;%ProgramFiles%\NASM"
  EXIT /B 0
)

IF EXIST "%ProgramFiles(x86)%\NASM\nasm.exe" (
  SET "Path=%Path%;%ProgramFiles(x86)%\NASM"
  EXIT /B 0
)

if EXIST "%LOCALAPPDATA%\bin\NASM\nasm.exe" (
  SET "Path=%Path%;%LOCALAPPDATA%\bin\NASM"
  EXIT /B 0
)

IF EXIST "%USERPROFILE%\NASM\nasm.exe" (
  ECHO Found NASM in %USERPROFILE%\NASM
  SET "Path=%Path%;%USERPROFILE%\NASM"
  EXIT /B 0
)

IF EXIST "%USERPROFILE%\PortableApps\NASM\nasm.exe" (
  ECHO Found NASM in %USERPROFILE%\PortableApps\NASM
  SET "Path=%Path%;%USERPROFILE%\PortableApps\NASM"
  EXIT /B 0
)

IF EXIST "%USERPROFILE%\scoop\apps\nasm\current\nasm.exe" (
  ECHO Found NASM in Scoop installation
  SET "Path=%Path%;%USERPROFILE%\scoop\apps\nasm\current"
  EXIT /B 0
)

IF EXIST "C:\ProgramData\chocolatey\bin\nasm.exe" (
  ECHO Found NASM in Chocolatey installation
  SET "Path=%Path%;C:\ProgramData\chocolatey\bin"
  EXIT /B 0
)

IF EXIST "C:\msys64\mingw64\bin\nasm.exe" (
  ECHO Found NASM in MSYS2 mingw64
  SET "Path=%Path%;C:\msys64\mingw64\bin"
  EXIT /B 0
)

IF EXIST "C:\msys64\mingw32\bin\nasm.exe" (
  ECHO Found NASM in MSYS2 mingw32
  SET "Path=%Path%;C:\msys64\mingw32\bin"
  EXIT /B 0
)

EXIT /B 1
