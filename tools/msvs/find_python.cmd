@IF NOT DEFINED DEBUG_HELPER @ECHO OFF

echo Looking for Python 2.x
setlocal enabledelayedexpansion

:: If python.exe is in %Path%, just validate
FOR /F "delims=" %%a IN ('where python.exe 2^> NUL') DO (
  SET need_path=0
  SET p=%%~dpa
  IF NOT ERRORLEVEL 1 GOTO :validate
)

:: Query the 3 locations mentioned in PEP 514 for a python2 InstallPath
FOR %%K IN ( "HKCU\Software", "HKLM\SOFTWARE", "HKLM\Software\Wow6432Node") DO (
  SET need_path=1
  CALL :find-versions-v2 %%K
  :: If validate returns 0 just jump to the end
  IF NOT ERRORLEVEL 1 GOTO :validate
)

goto :no-python


:: Find Python 2 installations in a registry location
:find-versions-v2
for /f "delims=" %%a in ('reg query "%~1\Python\PythonCore" /f * /k 2^> nul ^| findstr /r ^^HK ^| findstr "\\2\."') do (
  call :read-installpath %%a
  if not errorlevel 1 exit /b 0
)
exit /b 1

:: Read the InstallPath of a given Environment Key to %p%
:: https://www.python.org/dev/peps/pep-0514/#installpath
:read-installpath
:: %%a will receive token 3
:: %%b will receive *, corresponding to token 4 and all after
for /f "skip=2 tokens=3*" %%a in ('reg query "%1\InstallPath" /ve /t REG_SZ 2^> nul') do (
  set "head=%%a"
  set "tail=%%b"
  set "p=!head!"
  if not "!tail!"=="" set "p=!head! !tail!"
  exit /b 0
)
exit /b 1


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
