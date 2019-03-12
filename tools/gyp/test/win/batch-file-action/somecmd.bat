@echo off
:: The redirs to nul are important. %2 can end up being an unterminated "'d
:: string, so the remainder of the command line becomes the target file name,
:: which in turn fails because it's a filename containing >, nul, etc.
copy /y %1 %2 >nul 2>nul
