pushd %~dp0\..
python tools\test.py -J --mode=release %*
popd
