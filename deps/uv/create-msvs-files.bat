@echo off

cd %~dp0

if exist build\gyp goto have_gyp

echo svn co http://gyp.googlecode.com/svn/trunk@983 build/gyp
svn co http://gyp.googlecode.com/svn/trunk@983 build/gyp
if errorlevel 1 goto gyp_install_failed
goto have_gyp

:gyp_install_failed
echo Failed to download gyp. Make sure you have subversion installed, or 
echo manually install gyp into %~dp0build\gyp.
goto exit

:have_gyp
python gyp_uv
if not errorlevel 1 echo Done.

:exit
