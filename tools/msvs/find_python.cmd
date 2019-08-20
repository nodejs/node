@IF NOT DEFINED DEBUG_HELPER @ECHO OFF
echo Looking for Python 2.x
SETLOCAL
:: If python.exe is in %Path%, just validate
FOR /F "delims=" %%a IN ('where python.exe 2^> NUL') DO (
  SET need_path=0
  SET p=%%~dpa
  IF NOT ERRORLEVEL 1 GOTO :validate
)

:: Query the 3 locations mentioned in PEP 514 for a python2 InstallPath
FOR %%K IN ( "HKCU\Software", "HKLM\SOFTWARE", "HKLM\Software\Wow6432Node") DO (
  SET need_path=1
  CALL :find-main-branch %%K
  :: If validate returns 0 just jump to the end
  IF NOT ERRORLEVEL 1 GOTO :validate
)
goto :no-python

:: Helper subroutine to handle quotes in %1
:find-main-branch
SET main_key="%~1\Python\PythonCore"
REG QUERY %main_key% /s 2> NUL | findstr "2." | findstr InstallPath > NUL 2> NUL
IF NOT ERRORLEVEL 1 CALL :find-key %main_key%
EXIT /B

:: Query registry sub-tree for InstallPath
:find-key
FOR /F "delims=" %%a IN ('REG QUERY %1 /s 2^> NUL ^| findstr "2." ^| findstr InstallPath') DO IF NOT ERRORLEVEL 1 CALL :find-path %%a
EXIT /B

:: Parse the value of %1 as the path for python.exe
:find-path
FOR /F "tokens=3*" %%a IN ('REG QUERY %1 /ve') DO (
  SET pt=%%a
  IF NOT ERRORLEVEL 1 SET p=%pt%
  EXIT /B 0
)
EXIT /B 1

:: Check if %p% holds a path to a real python2 executable
:validate
IF NOT EXIST "%p%python.exe" goto :no-python
:: Check if %p% is python2
"%p%python.exe" -V 2>&1 | findstr /R "^Python.2.*" > NUL
IF ERRORLEVEL 1 goto :no-python2
:: We can wrap it up
ENDLOCAL & SET pt=%p%& SET need_path_ext=%need_path%
SET VCBUILD_PYTHON_LOCATION=%pt%python.exe
IF %need_path_ext%==1 SET Path=%Path%;%pt%
SET need_path_ext=
EXIT /B %ERRORLEVEL%

:no-python2
echo Python found in %p%, but it is not v2.x.
exit /B 1
:no-python
echo Could not find Python.
exit /B 1
