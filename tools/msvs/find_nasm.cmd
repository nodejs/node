@IF NOT DEFINED DEBUG_HELPER @ECHO OFF

echo Looking for NASM

for /f "delims=" %%a IN ('where nasm 2^> NUL') DO (
  echo NASM found in %%a
  exit /b 0
)

for %%a in ("%ProgramFiles%\NASM\nasm.exe" "%ProgramFiles(x86)%\NASM\nasm.exe" "%LOCALAPPDATA%\bin\NASM\nasm.exe") do (
  if exist %%a (
    call :find-nasm %%a
    exit /b 0
  )
)
exit /b 1

:find-nasm
set p=%~1
:: Remove the last nine characters, which are "\nasm.exe"
set p=%p:~0,-9%
set "Path=%Path%;%p%"
set p=
echo NASM found in %~1
exit /b 0
