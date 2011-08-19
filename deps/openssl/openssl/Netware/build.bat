@echo off

rem ========================================================================
rem   Batch file to automate building OpenSSL for NetWare.
rem
rem   usage:
rem      build [target] [debug opts] [assembly opts] [configure opts]
rem
rem      target        - "netware-clib" - CLib NetWare build (WinSock Sockets)
rem                    - "netware-clib-bsdsock" - CLib NetWare build (BSD Sockets)
rem                    - "netware-libc" - LibC NetWare build (WinSock Sockets)
rem                    - "netware-libc-bsdsock" - LibC NetWare build (BSD Sockets)
rem 
rem      debug opts    - "debug"  - build debug
rem
rem      assembly opts - "nw-mwasm" - use Metrowerks assembler
rem                    - "nw-nasm"  - use NASM assembler
rem                    - "no-asm"   - don't use assembly
rem
rem      configure opts- all unrecognized arguments are passed to the
rem                       perl configure script
rem
rem   If no arguments are specified the default is to build non-debug with
rem   no assembly.  NOTE: there is no default BLD_TARGET.
rem



rem   No assembly is the default - Uncomment section below to change
rem   the assembler default
set ASM_MODE=
set ASSEMBLER=
set NO_ASM=no-asm

rem   Uncomment to default to the Metrowerks assembler
rem set ASM_MODE=nw-mwasm
rem set ASSEMBLER=Metrowerks
rem set NO_ASM=

rem   Uncomment to default to the NASM assembler
rem set ASM_MODE=nw-nasm
rem set ASSEMBLER=NASM
rem set NO_ASM=

rem   No default Bld target
set BLD_TARGET=no_target
rem set BLD_TARGET=netware-clib
rem set BLD_TARGET=netware-libc


rem   Default to build non-debug
set DEBUG=
                                    
rem   Uncomment to default to debug build
rem set DEBUG=debug


set CONFIG_OPTS=
set ARG_PROCESSED=NO


rem   Process command line args
:opts
if "a%1" == "a" goto endopt
if "%1" == "no-asm"   set NO_ASM=no-asm
if "%1" == "no-asm"   set ARG_PROCESSED=YES
if "%1" == "debug"    set DEBUG=debug
if "%1" == "debug"    set ARG_PROCESSED=YES
if "%1" == "nw-nasm"  set ASM_MODE=nw-nasm
if "%1" == "nw-nasm"  set ASSEMBLER=NASM
if "%1" == "nw-nasm"  set NO_ASM=
if "%1" == "nw-nasm"  set ARG_PROCESSED=YES
if "%1" == "nw-mwasm" set ASM_MODE=nw-mwasm
if "%1" == "nw-mwasm" set ASSEMBLER=Metrowerks
if "%1" == "nw-mwasm" set NO_ASM=
if "%1" == "nw-mwasm" set ARG_PROCESSED=YES
if "%1" == "netware-clib" set BLD_TARGET=netware-clib
if "%1" == "netware-clib" set ARG_PROCESSED=YES
if "%1" == "netware-clib-bsdsock" set BLD_TARGET=netware-clib-bsdsock
if "%1" == "netware-clib-bsdsock" set ARG_PROCESSED=YES
if "%1" == "netware-libc" set BLD_TARGET=netware-libc
if "%1" == "netware-libc" set ARG_PROCESSED=YES
if "%1" == "netware-libc-bsdsock" set BLD_TARGET=netware-libc-bsdsock
if "%1" == "netware-libc-bsdsock" set ARG_PROCESSED=YES

rem   If we didn't recognize the argument, consider it an option for config
if "%ARG_PROCESSED%" == "NO" set CONFIG_OPTS=%CONFIG_OPTS% %1
if "%ARG_PROCESSED%" == "YES" set ARG_PROCESSED=NO

shift
goto opts
:endopt

rem make sure a valid BLD_TARGET was specified
if "%BLD_TARGET%" == "no_target" goto no_target

rem build the nlm make file name which includes target and debug info
set NLM_MAKE=
if "%BLD_TARGET%" == "netware-clib" set NLM_MAKE=netware\nlm_clib
if "%BLD_TARGET%" == "netware-clib-bsdsock" set NLM_MAKE=netware\nlm_clib_bsdsock
if "%BLD_TARGET%" == "netware-libc" set NLM_MAKE=netware\nlm_libc
if "%BLD_TARGET%" == "netware-libc-bsdsock" set NLM_MAKE=netware\nlm_libc_bsdsock
if "%DEBUG%" == "" set NLM_MAKE=%NLM_MAKE%.mak
if "%DEBUG%" == "debug" set NLM_MAKE=%NLM_MAKE%_dbg.mak

if "%NO_ASM%" == "no-asm" set ASM_MODE=
if "%NO_ASM%" == "no-asm" set ASSEMBLER=
if "%NO_ASM%" == "no-asm" set CONFIG_OPTS=%CONFIG_OPTS% no-asm
if "%NO_ASM%" == "no-asm" goto do_config


rem ==================================================
echo Generating x86 for %ASSEMBLER% assembler

echo Bignum
cd crypto\bn\asm
rem perl x86.pl %ASM_MODE% > bn-nw.asm
perl bn-586.pl %ASM_MODE% > bn-nw.asm
perl co-586.pl %ASM_MODE% > co-nw.asm
cd ..\..\..

echo AES
cd crypto\aes\asm
perl aes-586.pl %ASM_MODE% > a-nw.asm
cd ..\..\..

echo DES
cd crypto\des\asm
perl des-586.pl %ASM_MODE% > d-nw.asm
cd ..\..\..

echo "crypt(3)"

cd crypto\des\asm
perl crypt586.pl %ASM_MODE% > y-nw.asm
cd ..\..\..

echo Blowfish

cd crypto\bf\asm
perl bf-586.pl %ASM_MODE% > b-nw.asm
cd ..\..\..

echo CAST5
cd crypto\cast\asm
perl cast-586.pl %ASM_MODE% > c-nw.asm
cd ..\..\..

echo RC4
cd crypto\rc4\asm
perl rc4-586.pl %ASM_MODE% > r4-nw.asm
cd ..\..\..

echo MD5
cd crypto\md5\asm
perl md5-586.pl %ASM_MODE% > m5-nw.asm
cd ..\..\..

echo SHA1
cd crypto\sha\asm
perl sha1-586.pl %ASM_MODE% > s1-nw.asm
cd ..\..\..

echo RIPEMD160
cd crypto\ripemd\asm
perl rmd-586.pl %ASM_MODE% > rm-nw.asm
cd ..\..\..

echo RC5\32
cd crypto\rc5\asm
perl rc5-586.pl %ASM_MODE% > r5-nw.asm
cd ..\..\..

echo CPUID
cd crypto
perl x86cpuid.pl %ASM_MODE% > x86cpuid-nw.asm
cd ..\

rem ===============================================================
rem
:do_config

echo .
echo configure options: %CONFIG_OPTS% %BLD_TARGET%
echo .
perl configure %CONFIG_OPTS% %BLD_TARGET%

perl util\mkfiles.pl >MINFO

echo .
echo mk1mf.pl options: %DEBUG% %ASM_MODE% %CONFIG_OPTS% %BLD_TARGET%
echo .
perl util\mk1mf.pl %DEBUG% %ASM_MODE% %CONFIG_OPTS% %BLD_TARGET% >%NLM_MAKE%

make -f %NLM_MAKE% vclean
echo .
echo The makefile "%NLM_MAKE%" has been created use your maketool to
echo build (ex: make -f %NLM_MAKE%)
goto end

rem ===============================================================
rem
:no_target
echo .
echo .  No build target specified!!!
echo .
echo .  usage: build [target] [debug opts] [assembly opts] [configure opts]
echo .
echo .     target        - "netware-clib" - CLib NetWare build (WinSock Sockets)
echo .                   - "netware-clib-bsdsock" - CLib NetWare build (BSD Sockets)
echo .                   - "netware-libc" - LibC NetWare build (WinSock Sockets)
echo .                   - "netware-libc-bsdsock" - LibC NetWare build (BSD Sockets)
echo .
echo .     debug opts    - "debug"  - build debug
echo .
echo .     assembly opts - "nw-mwasm" - use Metrowerks assembler
echo .                     "nw-nasm"  - use NASM assembler
echo .                     "no-asm"   - don't use assembly
echo .
echo .     configure opts- all unrecognized arguments are passed to the
echo .                      perl configure script
echo .
echo .  If no debug or assembly opts are specified the default is to build
echo .  non-debug without assembly
echo .

        
:end        
