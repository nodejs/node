@echo off
rem batch file to build multi-thread test ( mttest.nlm )

rem command line arguments:
rem      debug => build using debug settings

rem
rem After building, copy mttest.nlm to the server and run it, you'll probably
rem want to redirect stdout and stderr.  An example command line would be
rem "mttest.nlm -thread 20 -loops 10 -CAfile \openssl\apps\server.pem >mttest.out 2>mttest.err"
rem 

del mttest.nlm

set BLD_DEBUG=
set CFLAGS=
set LFLAGS=
set LIBS=

if "%1" == "DEBUG" set BLD_DEBUG=YES
if "%1" == "debug" set BLD_DEBUG=YES

if "%MWCIncludes%" == "" goto inc_error
if "%PRELUDE%" == "" goto prelude_error
if "%IMPORTS%" == "" goto imports_error

set CFLAGS=-c -I..\..\outinc_nw -nosyspath -DOPENSSL_SYS_NETWARE -opt off -g -sym internal -maxerrors 20

if "%BLD_DEBUG%" == "YES" set LIBS=..\..\out_nw.dbg\ssl.lib ..\..\out_nw.dbg\crypto.lib
if "%BLD_DEBUG%" == ""  set LIBS=..\..\out_nw\ssl.lib ..\..\out_nw\crypto.lib

set LFLAGS=-msgstyle gcc -zerobss -stacksize 32768 -nostdlib -sym internal 
  
rem generate command file for metrowerks
echo.
echo Generating Metrowerks command file: mttest.def
echo # dynamically generated command file for metrowerks build > mttest.def
echo IMPORT @%IMPORTS%\clib.imp              >> mttest.def 
echo IMPORT @%IMPORTS%\threads.imp           >> mttest.def 
echo IMPORT @%IMPORTS%\ws2nlm.imp            >> mttest.def 
echo IMPORT GetProcessSwitchCount            >> mttest.def
echo MODULE clib                             >> mttest.def 

rem compile
echo.
echo Compiling mttest.c
mwccnlm.exe mttest.c %CFLAGS% 
if errorlevel 1 goto end

rem link               
echo.
echo Linking mttest.nlm
mwldnlm.exe %LFLAGS% -screenname mttest -commandfile mttest.def mttest.o "%PRELUDE%" %LIBS% -o mttest.nlm
if errorlevel 1 goto end

goto end

:inc_error
echo.
echo Environment variable MWCIncludes is not set - see install.nw
goto end

:prelude_error
echo.
echo Environment variable PRELUDE is not set - see install.nw
goto end

:imports_error
echo.
echo Environment variable IMPORTS is not set - see install.nw
goto end
    
    
:end
set BLD_DEBUG=
set CFLAGS=
set LFLAGS=
set LIBS=

