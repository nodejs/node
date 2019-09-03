@IF NOT DEFINED DEBUG_HELPER @ECHO OFF

echo Looking for Python
setlocal enabledelayedexpansion

:: To remove the preference for Python 2, but still support it, just remove
:: the 5 blocks marked with "Python 2:" and the support warnings

:: Python 2: If python.exe is in %Path%, use if it's Python 2
FOR /F "delims=" %%a IN ('where python.exe 2^> NUL') DO (
  SET need_path=0
  SET p=%%~dpa
  CALL :validate-v2
  IF NOT ERRORLEVEL 1 GOTO :found-python2
  GOTO :done-path-v2
)
:done-path-v2

:: Python 2: Query the 3 locations mentioned in PEP 514 for a python2 InstallPath
FOR %%K IN ( "HKCU\Software", "HKLM\SOFTWARE", "HKLM\Software\Wow6432Node") DO (
  SET need_path=1
  CALL :find-versions-v2 %%K
  IF NOT ERRORLEVEL 1 CALL :validate-v2
  IF NOT ERRORLEVEL 1 GOTO :found-python2
)

:: Use python.exe if in %PATH%
set need_path=0
for /f "delims=" %%a in ('where python.exe 2^> nul') do (
  set p=%%~dpa
  goto :found-python
)

:: Query the 3 locations mentioned in PEP 514 for a Python InstallPath
set need_path=1
for %%k in ( "HKCU\Software", "HKLM\SOFTWARE", "HKLM\Software\Wow6432Node") do (
  call :find-versions %%k
  if not errorlevel 1 goto :found-python
)

goto :no-python


:: Python 2: Find Python 2 installations in a registry location
:find-versions-v2
for /f "delims=" %%a in ('reg query "%~1\Python\PythonCore" /f * /k 2^> nul ^| findstr /r ^^HK ^| findstr "\\2\."') do (
  call :read-installpath %%a
  if not errorlevel 1 exit /b 0
)
exit /b 1

:: Find Python installations in a registry location
:find-versions
for /f "delims=" %%a in ('reg query "%~1\Python\PythonCore" /f * /k 2^> nul ^| findstr /r ^^HK') do (
  call :read-installpath %%a
  if not errorlevel 1 exit /b 0
)
exit /b 1

:: Read the InstallPath of a given Environment Key to %p%
:: https://www.python.org/dev/peps/pep-0514/#installpath
:read-installpath
:: %%a will receive everything before ), might have spaces depending on language
:: %%b will receive *, corresponding to everything after )
:: %%c will receive REG_SZ
:: %%d will receive the path, including spaces
for /f "skip=2 tokens=1* delims=)" %%a in ('reg query "%1\InstallPath" /ve /t REG_SZ 2^> nul') do (
  for /f "tokens=1*" %%c in ("%%b") do (
    if not "%%c"=="REG_SZ" exit /b 1
    set "p=%%d"
    exit /b 0
  )
)
exit /b 1


:: Python 2: Check if %p% holds a path to a real python2 executable
:validate-v2
IF NOT EXIST "%p%\python.exe" EXIT /B 1
:: Check if %p% is python2
"%p%\python.exe" -V 2>&1 | findstr /R "^Python.2.*" > NUL
EXIT /B %ERRORLEVEL%


:: Python 2:
:found-python2
echo Python 2 found in %p%\python.exe
set pyver=2
goto :done

:found-python
echo Python found in %p%\python.exe
echo WARNING: Python 3 is not yet fully supported, to avoid issues Python 2 should be installed.
set pyver=3
goto :done

:done
endlocal ^
  & set "pt=%p%" ^
  & set "need_path_ext=%need_path%" ^
  & set "VCBUILD_PYTHON_VERSION=%pyver%"
set "VCBUILD_PYTHON_LOCATION=%pt%\python.exe"
if %need_path_ext%==1 set "PATH=%pt%;%PATH%"
set "pt="
set "need_path_ext="
exit /b 0

:no-python
echo Could not find Python.
exit /b 1
