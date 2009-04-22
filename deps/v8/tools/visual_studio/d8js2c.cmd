@echo off
set SOURCE_DIR=%1
set TARGET_DIR=%2
set PYTHON="..\..\..\third_party\python_24\python.exe"
if not exist %PYTHON% set PYTHON=python.exe
%PYTHON% ..\js2c.py %TARGET_DIR%\natives.cc %TARGET_DIR%\natives-empty.cc D8 %SOURCE_DIR%\macros.py %SOURCE_DIR%\d8.js
