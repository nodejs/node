@echo off
setlocal

SET tools_dir=%~dp0
SET v8_tools=%tools_dir%..\..\deps\v8\tools\

SET temp_script=%TEMP%\node-tick-processor-input-script

IF NOT DEFINED NODE (SET NODE=node.exe)
%NODE% --version 2> NUL
if %ERRORLEVEL%==9009 (SET NODE=%~dp0\..\..\Release\iojs.exe)


type %tools_dir%polyfill.js %v8_tools%splaytree.js %v8_tools%codemap.js^
 %v8_tools%csvparser.js %v8_tools%consarray.js %v8_tools%profile.js^
 %v8_tools%profile_view.js %v8_tools%logreader.js %v8_tools%SourceMap.js^
 %v8_tools%tickprocessor.js %v8_tools%tickprocessor-driver.js >> %temp_script%
%NODE% %temp_script% --windows %*
del %temp_script%
