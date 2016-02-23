@echo off

rem ========================================================================
rem   Batch file to assist in setting up the necessary enviroment for
rem   building OpenSSL for NetWare.
rem
rem   usage:
rem      set_env [target]
rem
rem      target      - "netware-clib" - Clib build
rem                  - "netware-libc" - LibC build
rem
rem

if "a%1" == "a" goto usage
               
set LIBC_BUILD=
set CLIB_BUILD=
set GNUC=

if "%1" == "netware-clib" set CLIB_BUILD=Y
if "%1" == "netware-clib" set LIBC_BUILD=

if "%1" == "netware-libc" set LIBC_BUILD=Y
if "%1" == "netware-libc" set CLIB_BUILD=

if "%2" == "gnuc" set GNUC=Y
if "%2" == "codewarrior" set GNUC=

rem   Location of tools (compiler, linker, etc)
if "%NDKBASE%" == "" set NDKBASE=c:\Novell

rem   If Perl for Win32 is not already in your path, add it here
set PERL_PATH=

rem   Define path to the Metrowerks command line tools
rem   or GNU Crosscompiler gcc / nlmconv
rem   ( compiler, assembler, linker)
if "%GNUC%" == "Y" set COMPILER_PATH=c:\usr\i586-netware\bin;c:\usr\bin
if "%GNUC%" == "" set COMPILER_PATH=c:\prg\cwcmdl40

rem   If using gnu make define path to utility
rem set GNU_MAKE_PATH=%NDKBASE%\gnu
set GNU_MAKE_PATH=c:\prg\tools

rem   If using ms nmake define path to nmake
rem set MS_NMAKE_PATH=%NDKBASE%\msvc\600\bin

rem   If using NASM assembler define path
rem set NASM_PATH=%NDKBASE%\nasm
set NASM_PATH=c:\prg\tools

rem   Update path to include tool paths
set path=%path%;%COMPILER_PATH%
if not "%GNU_MAKE_PATH%" == "" set path=%path%;%GNU_MAKE_PATH%
if not "%MS_NMAKE_PATH%" == "" set path=%path%;%MS_NMAKE_PATH%
if not "%NASM_PATH%"     == "" set path=%path%;%NASM_PATH%
if not "%PERL_PATH%"     == "" set path=%path%;%PERL_PATH%

rem   Set INCLUDES to location of Novell NDK includes
if "%LIBC_BUILD%" == "Y" set INCLUDE=%NDKBASE%\ndk\libc\include;%NDKBASE%\ndk\libc\include\winsock
if "%CLIB_BUILD%" == "Y" set INCLUDE=%NDKBASE%\ndk\nwsdk\include\nlm;%NDKBASE%\ws295sdk\include

rem   Set Imports to location of Novell NDK import files
if "%LIBC_BUILD%" == "Y" set IMPORTS=%NDKBASE%\ndk\libc\imports
if "%CLIB_BUILD%" == "Y" set IMPORTS=%NDKBASE%\ndk\nwsdk\imports

rem   Set PRELUDE to the absolute path of the prelude object to link with in
rem   the Metrowerks NetWare PDK - NOTE: for Clib builds "clibpre.o" is 
rem   recommended, for LibC NKS builds libcpre.o must be used
if "%GNUC%" == "Y" goto gnuc
if "%LIBC_BUILD%" == "Y" set PRELUDE=%IMPORTS%\libcpre.o
rem if "%CLIB_BUILD%" == "Y" set PRELUDE=%IMPORTS%\clibpre.o
if "%CLIB_BUILD%" == "Y" set PRELUDE=%IMPORTS%\prelude.o
echo using MetroWerks CodeWarrior 
goto info

:gnuc
if "%LIBC_BUILD%" == "Y" set PRELUDE=%IMPORTS%\libcpre.gcc.o
rem if "%CLIB_BUILD%" == "Y" set PRELUDE=%IMPORTS%\clibpre.gcc.o
if "%CLIB_BUILD%" == "Y" set PRELUDE=%IMPORTS%\prelude.gcc.o
echo using GNU GCC Compiler 

:info
echo.

if "%LIBC_BUILD%" == "Y" echo Enviroment configured for LibC build
if "%LIBC_BUILD%" == "Y" echo use "netware\build.bat netware-libc ..." 

if "%CLIB_BUILD%" == "Y" echo Enviroment configured for CLib build
if "%CLIB_BUILD%" == "Y" echo use "netware\build.bat netware-clib ..." 

goto end

:usage
rem ===============================================================
echo.
echo No target build specified!
echo.
echo usage: set_env [target] [compiler]
echo.
echo target      - "netware-clib" - Clib build
echo             - "netware-libc" - LibC build
echo.
echo compiler    - "gnuc"         - GNU GCC Compiler
echo             - "codewarrior"  - MetroWerks CodeWarrior (default)
echo.

:end
echo.


