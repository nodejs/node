::-------------------------------------------------------------------------------------------------------
:: Copyright (C) Microsoft. All rights reserved.
:: Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
::-------------------------------------------------------------------------------------------------------
:: ============================================================================
::
:: runtests.cmd
::
:: Runs checkin tests using the ch.exe on the path, in 2 variants:
::
:: -maxInterpretCount:1 -maxSimpleJitRunCount:1 -bgjit-
:: <dynapogo>
::
:: Logs are placed into:
::
:: logs\interpreted
:: logs\dynapogo
::
:: User specified variants:
:: logs\forcedeferparse
:: logs\nodeferparse
:: logs\forceundodefer
:: logs\bytecodeserialized (serialized to byte codes)
:: logs\forceserialized (force bytecode serialization internally)
::
:: ============================================================================
@echo off
setlocal

goto :main

:: ============================================================================
:: Print usage
:: ============================================================================
:printUsage

  echo runtests.cmd -x86^|-x64^|-arm -debug^|-test [options]
  echo.
  echo Required switches:
  echo.
  echo   Specify architecture of ChakraCore:
  echo.
  echo   -x86           Build arch of binaries is x86
  echo   -x64           Build arch of binaries is x64
  echo   -arm           Build arch of binaries is ARM
  echo.
  echo   Specify type of ChakraCore:
  echo.
  echo   -debug         Build type of binaries is debug
  echo   -test          Build type of binaries is test
  echo   -codecoverage  Build type of binaries is codecoverage
  echo.
  echo   Shorthand combinations can be used, e.g. -x64debug
  echo.
  echo   Note: release build type currently unsupported by ch.exe
  echo.
  echo Options:
  echo.
  echo   -dirs dirname  Run only the specified directory
  :: TODO Add more usage help

  goto :eof

:: ============================================================================
:: Print how to get help
:: ============================================================================
:printGetHelp

  echo For help use runtests.cmd -?

  goto :eof

:: ============================================================================
:: Main script
:: ============================================================================
:main

  if not exist %cd%\rlexedirs.xml (
    echo Error: rlexedirs.xml not found in current directory.
    echo runtests.cmd must be run from a test root directory containing rlexedirs.xml.
    exit /b 1
  )

  call :initVars
  call :parseArgs %*

  if not "%fShowUsage%" == "" (
    call :printUsage
    goto :eof
  )

  call :validateArgs

  if not "%fShowGetHelp%" == "" (
    call :printGetHelp
    goto :eof
  )

  call :configureVars

  set _logsRoot=%cd%\logs
  call :doSilent del /s /q profile.dpl.*

  for %%i in (%_Variants%) do (
    set _TESTCONFIG=%%i
    call :RunOneVariant
  )

  call :cleanUp

  for %%i in (%_Variants%) do (
    echo.
    echo ######## Logs for %%i variant ########
    if exist %_logsRoot%\%_BuildArch%_%_BuildType%\%%i\rl.log (
      type %_logsRoot%\%_BuildArch%_%_BuildType%\%%i\rl.log
    ) else (
      echo ERROR: Log file '%_logsRoot%\%_BuildArch%_%_BuildType%\%%i\rl.log' does not exist
    )
  )

  exit /b %_HadFailures%

:: ============================================================================
:: Parse the user arguments into environment variables
:: ============================================================================
:parseArgs

  :NextArgument

  if "%1" == "-?" set fShowUsage=1& goto :ArgOk
  if "%1" == "/?" set fShowUsage=1& goto :ArgOk

  if /i "%1" == "-x86"              set _BuildArch=x86&                                         goto :ArgOk
  if /i "%1" == "-x64"              set _BuildArch=x64&                                         goto :ArgOk
  if /i "%1" == "-arm"              set _BuildArch=arm&                                         goto :ArgOk
  if /i "%1" == "-debug"            set _BuildType=debug&                                       goto :ArgOk
  if /i "%1" == "-test"             set _BuildType=test&                                        goto :ArgOk
  if /i "%1" == "-codecoverage"     set _BuildType=codecoverage&                                goto :ArgOk

  if /i "%1" == "-x86debug"         set _BuildArch=x86&set _BuildType=debug&                    goto :ArgOk
  if /i "%1" == "-x64debug"         set _BuildArch=x64&set _BuildType=debug&                    goto :ArgOk
  if /i "%1" == "-armdebug"         set _BuildArch=arm&set _BuildType=debug&                    goto :ArgOk
  if /i "%1" == "-x86test"          set _BuildArch=x86&set _BuildType=test&                     goto :ArgOk
  if /i "%1" == "-x64test"          set _BuildArch=x64&set _BuildType=test&                     goto :ArgOk
  if /i "%1" == "-armtest"          set _BuildArch=arm&set _BuildType=test&                     goto :ArgOk
  if /i "%1" == "-x86codecoverage"  set _BuildArch=x86&set _BuildType=codecoverage&             goto :ArgOk
  if /i "%1" == "-x64codecoverage"  set _BuildArch=x64&set _BuildType=codecoverage&             goto :ArgOk
  if /i "%1" == "-armcodecoverage"  set _BuildArch=arm&set _BuildType=codecoverage&             goto :ArgOk

  if /i "%1" == "-binary"           set _Binary=-binary:%2&                                     goto :ArgOkShift2
  if /i "%1" == "-bindir"           set _BinDir=%~f2&                                           goto :ArgOkShift2
  if /i "%1" == "-dirs"             set _DIRS=-dirs:%~2&                                        goto :ArgOkShift2
  if /i "%1" == "-win7"             set TARGET_OS=win7&                                         goto :ArgOk
  if /i "%1" == "-win8"             set TARGET_OS=win8&                                         goto :ArgOk
  if /i "%1" == "-winBlue"          set TARGET_OS=winBlue&                                      goto :ArgOk
  if /i "%1" == "-win10"            set TARGET_OS=win10&                                        goto :ArgOk
  if /i "%1" == "-nottags"          set _NOTTAGS=%_NOTTAGS% -nottags:%~2&                       goto :ArgOkShift2
  if /i "%1" == "-tags"             set _TAGS=%_TAGS% -tags:%~2&                                goto :ArgOkShift2
  if /i "%1" == "-dirtags"          set _DIRTAGS=%_DIRTAGS% -dirtags:%~2&                       goto :ArgOkShift2
  if /i "%1" == "-dirnottags"       set _DIRNOTTAGS=%_DIRNOTTAGS% -dirnottags:%~2&              goto :ArgOkShift2
  if /i "%1" == "-includeSlow"      set _includeSlow=1&                                         goto :ArgOk
  if /i "%1" == "-onlySlow"         set _onlySlow=1&                                            goto :ArgOk
  if /i "%1" == "-quiet"            set _quiet=-quiet&                                          goto :ArgOk
  :: TODO Consider removing -drt and exclude_drt in some reasonable manner
  if /i "%1" == "-drt"              set _drt=1& set _NOTTAGS=%_NOTTAGS% -nottags:exclude_drt&   goto :ArgOk
  if /i "%1" == "-nightly"          set _nightly=1&                                             goto :ArgOk
  if /i "%1" == "-rebase"           set _rebase=-rebase&                                        goto :ArgOk
  if /i "%1" == "-rundebug"         set _RUNDEBUG=1&                                            goto :ArgOk
  :: TODO Figure out best way to specify build arch for tests that are excluded to specific archs
  if /i "%1" == "-platform"         set _buildArch=%2&                                          goto :ArgOkShift2
  :: TODO Figure out best way to specify build type for tests that are excluded to specific type (chk, fre, etc)
  if /i "%1" == "-buildType"        set _buildType=%2&                                          goto :ArgOkShift2
  if /i "%1" == "-binaryRoot"       set _binaryRoot=%~f2&                                       goto :ArgOkShift2
  if /i "%1" == "-variants"         set _Variants=%~2&                                          goto :ArgOkShift2
  if /i "%1" == "-cleanupall"       set _CleanUpAll=1&                                          goto :ArgOk

  if /i "%1" == "-extraVariants" (
    :: Extra variants are specified by the user but not run by default.
    if "%_ExtraVariants%" == "" (
      set _ExtraVariants=%~2
    ) else (
      set _ExtraVariants=%_ExtraVariants%,%~2
    )
    goto :ArgOkShift2
  )

  :: Defined here are shorthand versions for specifying
  :: extra variants when running.
  if /i "%1" == "-parser" (
    if "%_ExtraVariants%" == "" (
      set _ExtraVariants=forcedeferparse,nodeferparse,forceundodefer
    ) else (
      set _ExtraVariants=%_ExtraVariants%,forcedeferparse,nodeferparse,forceundodefer
    )
    goto :ArgOk
  )
  if /i "%1" == "-serialization" (
    if "%_ExtraVariants%" == "" (
      set _ExtraVariants=bytecodeserialized,forceserialized
    ) else (
      set _ExtraVariants=%_ExtraVariants%,bytecodeserialized,forceserialized
    )
    goto :ArgOk
  )
  if /i "%1" == "-disablejit" (
    set _DisableJit=1
    set _Variants=disable_jit
    goto :ArgOk
  )

  if not "%1" == "" echo Unknown argument: %1 & set fShowGetHelp=1

  goto :eof

  :ArgOkShift2
  shift

  :ArgOk
  shift

  goto :NextArgument

:: ============================================================================
:: Initialize batch script variables to defaults
:: ============================================================================
:initVars

  set _HadFailures=0
  set _RootDir=%~dp0..
  set _BinDir=%_RootDir%\Build\VcBuild\Bin
  set _BuildArch=
  set _BuildType=
  set _Binary=-binary:ch.exe
  set _Variants=
  set _TAGS=
  set _NOTTAGS=
  set _DIRNOTTAGS=
  set _DIRTAGS=
  set _drt=
  set _rebase=
  set _ExtraVariants=
  set _dynamicprofilecache=-dynamicprofilecache:profile.dpl
  set _dynamicprofileinput=-dynamicprofileinput:profile.dpl
  set _includeSlow=
  set _onlySlow=
  set _CleanUpAll=
  set _nightly=
  set TARGET_OS=win10
  set _quiet=

  goto :eof

:: ============================================================================
:: Validate that required arguments were specified
:: ============================================================================
:validateArgs

  if "%_BuildArch%" == "" (
    echo Error missing required build architecture or build type switch
    set fShowGetHelp=1
    goto :eof
  )

  if "%_BuildType%" == "" (
    echo Error missing required build architecture or build type switch
    set fShowGetHelp=1
  )

  if not exist %_binDir%\%_BuildArch%_%_BuildType%\%_Binary:~8% (
    echo Error missing binary %_binDir%\%_BuildArch%_%_BuildType%\%_Binary:~8%
    set fShowGetHelp=1
  )

  goto :eof

:: ============================================================================
:: Configure the script variables and environment based on parsed arguments
:: ============================================================================
:configureVars

  echo Adding to PATH: %_binDir%\%_BuildArch%_%_BuildType%
  set path=%_binDir%\%_BuildArch%_%_BuildType%;%path%

  :: If the user didn't specify explicit variants then do the defaults
  if "%_Variants%"=="" set _Variants=interpreted,dynapogo

  :: If the user specified extra variants to run (i.e. in addition to the defaults), include them.
  if not "%_ExtraVariants%" == "" set _Variants=%_Variants%,%_ExtraVariants%

  rem TODO: Move any apollo tests from core\test back into private unittests
  set _ExcludeApolloTests=
  if "%APOLLO%" == "1" (
    set _ExcludeApolloTests=-nottags:exclude_apollo
    set TARGET_OS=wp8
  )

  if not "%_nightly%" == "1" (
    set _NOTTAGS=%_NOTTAGS% -nottags:nightly
  ) else (
    set _NOTTAGS=%_NOTTAGS% -nottags:exclude_nightly
  )

  if "%_includeSlow%%_onlySlow%" == "" (
    set _NOTTAGS=%_NOTTAGS% -nottags:Slow
  )
  if "%_onlySlow%" == "1" (
    set _TAGS=%_TAGS% -tags:Slow
  )

  if not "%NUM_RL_THREADS%" == "" (
    set _RL_THREAD_FLAGS=-threads:%NUM_RL_THREADS%
  )

  if "%_DIRS%" == "" (
    set _DIRS=-all
  )

  set _BuildArchMapped=%_BuildArch%
  set _BuildTypeMapped=%_BuildType%

  :: Map new build arch and type names to old names until rl test tags are
  :: updated to the new names
  if "%_BuildArchMapped%" == "x64" set _BuildArchMapped=amd64
  if "%_BuildTypeMapped%" == "debug" set _BuildTypeMapped=chk
  if "%_BuildTypeMapped%" == "test" set _BuildTypeMapped=fre
  if "%_BuildTypeMapped%" == "codecoverage" set _BuildTypeMapped=fre

  if "%Disable_JIT%" == "1" (
      set _dynamicprofilecache=
      set _dynamicprofileinput=
  )
  goto :eof

:: ============================================================================
:: Run one variant
:: ============================================================================
:RunOneVariant

  set _OLD_CC_FLAGS=%EXTRA_CC_FLAGS%
  set EXTRA_RL_FLAGS=-appendtestnametoextraccflags
  set _exclude_serialized=

  if "%_BuildType%" == "debug" (
    rem Enabling storing dumps on user directory.
    set EXTRA_CC_FLAGS=%EXTRA_CC_FLAGS% -DumpOnCrash
  )

  if "%_Binary%" == "-binary:ch.exe" (
    set EXTRA_CC_FLAGS=%EXTRA_CC_FLAGS% -WERExceptionSupport -ExtendedErrorStackForTestHost
  )

  if "%_TESTCONFIG%"=="interpreted" (
    set EXTRA_CC_FLAGS=%EXTRA_CC_FLAGS% -maxInterpretCount:1 -maxSimpleJitRunCount:1 -bgjit- %_dynamicprofilecache%
  )
  if "%_TESTCONFIG%"=="nonative" (
    set EXTRA_CC_FLAGS=%EXTRA_CC_FLAGS% -nonative
    set EXTRA_RL_FLAGS=-nottags:exclude_interpreted -nottags:fails_interpreted
  )
  :: DisableJit is different from NoNative in that NoNative can be used 
  :: with builds which include backend code, whereas DisableJit doesn't have
  :: backend code linked in, and also disables other features that incidentally
  :: depends on the backend like dynamic profile, asmjs, simdjs, background parsing etc.
  :: TODO: Re-enable interpreter mode asmjs and simdjs
  if "%_TESTCONFIG%"=="disable_jit" (
    set EXTRA_CC_FLAGS=%EXTRA_CC_FLAGS% -nonative
    set EXTRA_RL_FLAGS=-nottags:exclude_interpreted -nottags:fails_interpreted -nottags:require_backend
  )
  if "%_TESTCONFIG%"=="dynapogo"    (
    set EXTRA_CC_FLAGS=%EXTRA_CC_FLAGS% -forceNative -off:simpleJit -bgJitDelay:0 %_dynamicprofileinput%
  )

  :: Variants after here are user supplied variants (not run by default).
  if "%_TESTCONFIG%"=="forcedeferparse" (
    set EXTRA_CC_FLAGS=%EXTRA_CC_FLAGS% -forceDeferParse %_dynamicprofilecache%
    set _exclude_forcedeferparse=-nottags:exclude_forcedeferparse
  )
  if "%_TESTCONFIG%"=="nodeferparse" (
    set EXTRA_CC_FLAGS=%EXTRA_CC_FLAGS% -noDeferParse %_dynamicprofilecache%
    set _exclude_nodeferparse=-nottags:exclude_nodeferparse
  )
  if "%_TESTCONFIG%"=="forceundodefer" (
    set EXTRA_CC_FLAGS=%EXTRA_CC_FLAGS% -forceUndoDefer %_dynamicprofilecache%
    set _exclude_forceundodefer=-nottags:exclude_forceundodefer
  )
  if "%_TESTCONFIG%"=="bytecodeserialized" (
    set EXTRA_CC_FLAGS=%EXTRA_CC_FLAGS% -recreatebytecodefile -serialized:%TEMP%\ByteCode
    set _exclude_serialized=-nottags:exclude_serialized
  )
  if "%_TESTCONFIG%"=="forceserialized" (
    set EXTRA_CC_FLAGS=%EXTRA_CC_FLAGS% -forceserialized
    set EXTRA_RL_FLAGS=
    set _exclude_serialized=-nottags:exclude_serialized
  )

  echo.
  echo ############# Starting %_TESTCONFIG% variant #############

  call :doSilent del /q %_logsRoot%\%_BuildArch%_%_BuildType%\%_TESTCONFIG%\rl*
  call :doSilent md %_logsRoot%\%_BuildArch%_%_BuildType%\%_TESTCONFIG%

  set _rlArgs=%_Binary%
  set _rlArgs=%_rlArgs% -target:%_BuildArchMapped%
  set _rlArgs=%_rlArgs% -nottags:fail
  set _rlArgs=%_rlArgs% %_RL_THREAD_FLAGS%
  set _rlArgs=%_rlArgs% %_DIRS%
  set _rlArgs=%_rlArgs% -verbose
  set _rlArgs=%_rlArgs% %_TAGS%
  set _rlArgs=%_rlArgs% %_NOTTAGS%
  set _rlArgs=%_rlArgs% %_DIRTAGS%
  set _rlArgs=%_rlArgs% %_DIRNOTTAGS%
  set _rlArgs=%_rlArgs% -nottags:fails_%_TESTCONFIG%
  set _rlArgs=%_rlArgs% -nottags:exclude_%_TESTCONFIG%
  set _rlArgs=%_rlArgs% -nottags:exclude_%TARGET_OS%
  set _rlArgs=%_rlArgs% -nottags:exclude_%_BuildArchMapped%
  set _rlArgs=%_rlArgs% -nottags:exclude_%_BuildTypeMapped%
  set _rlArgs=%_rlArgs% %_exclude_serialized%
  set _rlArgs=%_rlArgs% %_exclude_forcedeferparse%
  set _rlArgs=%_rlArgs% %_exclude_nodeferparse%
  set _rlArgs=%_rlArgs% %_exclude_forceundodefer%
  set _rlArgs=%_rlArgs% %_ExcludeApolloTests%
  set _rlArgs=%_rlArgs% %_quiet%
  set _rlArgs=%_rlArgs% -exe
  set _rlArgs=%_rlArgs% %EXTRA_RL_FLAGS%
  set _rlArgs=%_rlArgs% %_rebase%

  set REGRESS=%CD%

  call :do rl %_rlArgs%
  if ERRORLEVEL 1 set _HadFailures=1

  call :do move /Y %_logsRoot%\*.log %_logsRoot%\%_BuildArch%_%_BuildType%\%_TESTCONFIG%

  set EXTRA_CC_FLAGS=%_OLD_CC_FLAGS%

  goto :eof

:: ============================================================================
:: Clean up left over files
:: ============================================================================
:cleanUp

  call :doSilent del /s *.bc
  call :doSilent del /s *.out
  call :doSilent del /s *.dpl
  call :doSilent del /s profile.dpl.*
  call :doSilent del /s testout*

  if "%_CleanUpAll%" == "1" (
    call :doSilent del /s *.rebase
  )

  goto :eof

:: ============================================================================
:: Echo a command line before executing it
:: ============================================================================
:do

  echo ^>^> %*
  cmd /s /c "%*"

  goto :eof

:: ============================================================================
:: Echo a command line before executing it and redirect the command's output
:: to nul
:: ============================================================================
:doSilent

  echo ^>^> %* ^> nul 2^>^&1
  cmd /s /c "%* > nul 2>&1"

  goto :eof
