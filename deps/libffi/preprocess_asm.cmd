@echo off

cl /nologo /EP /I%1 /I%2 /DPIC /DFFI_BUILDING /DHAVECONFIG_H %3 > %4
