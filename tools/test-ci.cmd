pushd %~dp0\..
python tools\test.py -J --mode=release %*
set TEST_CI_EXIT=%ERRORLEVEL%
popd
exit /b %TEST_CI_EXIT%
