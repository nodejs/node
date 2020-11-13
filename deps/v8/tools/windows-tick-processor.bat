@echo off

SET tools_dir=%~dp0
IF 1%D8_PATH% == 1 (SET D8_PATH=%tools_dir%..)

SET log_file=v8.log

rem find the name of the log file to process, it must not start with a dash.
rem we prepend cmdline args with a number (in fact, any letter or number)
rem to cope with empty arguments.
SET arg1=1%1
IF NOT %arg1:~0,2% == 1 (IF NOT %arg1:~0,2% == 1- SET log_file=%1)
SET arg2=2%2
IF NOT %arg2:~0,2% == 2 (IF NOT %arg2:~0,2% == 2- SET log_file=%2)
SET arg3=3%3
IF NOT %arg3:~0,2% == 3 (IF NOT %arg3:~0,2% == 3- SET log_file=%3)
SET arg4=4%4
IF NOT %arg4:~0,2% == 4 (IF NOT %arg4:~0,2% == 4- SET log_file=%4)
SET arg5=5%5
IF NOT %arg5:~0,2% == 5 (IF NOT %arg5:~0,2% == 5- SET log_file=%5)
SET arg6=6%6
IF NOT %arg6:~0,2% == 6 (IF NOT %arg6:~0,2% == 6- SET log_file=%6)
SET arg7=7%7
IF NOT %arg7:~0,2% == 7 (IF NOT %arg7:~0,2% == 7- SET log_file=%7)
SET arg8=8%8
IF NOT %arg8:~0,2% == 8 (IF NOT %arg8:~0,2% == 8- SET log_file=%8)
SET arg9=9%9
IF NOT %arg9:~0,2% == 9 (IF NOT %arg9:~0,2% == 9- SET log_file=%9)

type %log_file% | %D8_PATH%\d8 --module %tools_dir%tickprocessor-driver.js -- --windows %*
