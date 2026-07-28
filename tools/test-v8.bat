if not defined DEPOT_TOOLS_PATH goto depot-tools-not-found
if "%config%"=="Debug" set test_args=%target_arch%.debug
if "%config%"=="Release" set test_args=%target_arch%.release
set savedpath=%path%
set path=%DEPOT_TOOLS_PATH%;%path%
pushd .

set ERROR_STATUS=0
echo calling: tools\make-v8.sh
sh tools\make-v8.sh
if errorlevel 1 set ERROR_STATUS=1&goto test-v8-exit
cd %~dp0
cd deps\v8
echo running 'python tools\dev\v8gen.py %test_args%'
call python tools\dev\v8gen.py %test_args%
if errorlevel 1 set ERROR_STATUS=1&goto test-v8-exit
echo running 'ninja -C out.gn/%test_args% %v8_build_options%'
call ninja -C out.gn/%test_args% %v8_build_options%
if errorlevel 1 set ERROR_STATUS=1&goto test-v8-exit
set path=%savedpath%

if not defined test_v8 goto test-v8-intl
echo running 'python tools\run-tests.py %common_v8_test_options% %v8_test_options% --slow-tests-cutoff 1000000 --json-test-results v8-tap.xml'
call python tools\run-tests.py %common_v8_test_options% %v8_test_options% --slow-tests-cutoff 1000000 --json-test-results v8-tap.xml
call python ..\..\tools\v8-json-to-junit.py < v8-tap.xml > v8-tap.json

:test-v8-intl
if not defined test_v8_intl goto test-v8-benchmarks
echo running 'python tools\run-tests.py %common_v8_test_options% intl --slow-tests-cutoff 1000000 --json-test-results v8-intl-tap.xml'
call python tools\run-tests.py %common_v8_test_options% intl --slow-tests-cutoff 1000000 --json-test-results ./v8-intl-tap.xml
call python ..\..\tools\v8-json-to-junit.py < v8-intl-tap.xml > v8-intl-tap.json

:test-v8-benchmarks
if not defined test_v8_benchmarks goto test-v8-exit
echo running 'python tools\run-tests.py %common_v8_test_options% benchmarks --slow-tests-cutoff 1000000 --json-test-results v8-benchmarks-tap.xml'
call python tools\run-tests.py %common_v8_test_options% benchmarks --slow-tests-cutoff 1000000 --json-test-results ./v8-benchmarks-tap.xml
call python ..\..\tools\v8-json-to-junit.py < v8-benchmarks-tap.xml > v8-benchmarks-tap.json
goto test-v8-exit

:test-v8-exit
popd
if defined savedpath set path=%savedpath%
exit /b %ERROR_STATUS%

:depot-tools-not-found
echo Failed to find a suitable depot tools to test v8
exit /b 1

