@SETLOCAL
@IF EXIST "%~dp0\node.exe" (
  "%~dp0\node.exe"  "%~dp0\..\dist\npm.js" %*
) ELSE (
  @SET PATHEXT=%PATHEXT:;.JS;=;%
  node  "%~dp0\..\dist\npm.js" %*
)
