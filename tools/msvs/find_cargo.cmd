@IF NOT DEFINED DEBUG_HELPER @ECHO OFF
SETLOCAL

ECHO Looking for cargo

REM Look in the same places as deps\crates\cargo_build.py.
SET "cargo_cmd=cargo"
IF DEFINED CARGO SET "cargo_cmd=%CARGO%"

IF EXIST "%cargo_cmd%" EXIT /B 0

FOR /F "delims=" %%a IN ('where "%cargo_cmd%" 2^> NUL') DO (
  EXIT /B 0
)

IF EXIST "%USERPROFILE%\.cargo\bin\cargo.exe" EXIT /B 0

EXIT /B 1
