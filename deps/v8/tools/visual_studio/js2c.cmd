@echo off
set SOURCE_DIR=%1
set TARGET_DIR=%2
set PYTHON="..\..\..\third_party\python_24\python.exe"
if not exist %PYTHON% set PYTHON=python.exe
%PYTHON% ..\js2c.py %TARGET_DIR%\natives.cc %TARGET_DIR%\natives-empty.cc CORE %SOURCE_DIR%\macros.py %SOURCE_DIR%\runtime.js %SOURCE_DIR%\v8natives.js %SOURCE_DIR%\array.js %SOURCE_DIR%\string.js %SOURCE_DIR%\uri.js %SOURCE_DIR%\math.js %SOURCE_DIR%\messages.js %SOURCE_DIR%\apinatives.js %SOURCE_DIR%\debug-delay.js %SOURCE_DIR%\mirror-delay.js %SOURCE_DIR%\date-delay.js %SOURCE_DIR%\regexp-delay.js
