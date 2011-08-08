@REM Hello World

cd %~dp0

IF EXIST %~dp0build\gyp GOTO WINDIR


svn co http://gyp.googlecode.com/svn/trunk@983 build/gyp

:WINDIR

@python build\gyp_uv


