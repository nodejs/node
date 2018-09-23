rem win32 dll build

set OPTIONS1=-DDES_ASM -DBN_ASM -DBF_ASM -DFLAT_INC -Iout -Itmp -DL_ENDIAN
set OPTIONS2=/W3 /WX /Ox /Gs0 /GF /Gy /nologo

set OPTIONS=%OPTIONS1% %OPTIONS2%

rem ml /coff /c crypto\bf\asm\b-win32.asm
rem ml /coff /c crypto\des\asm\c-win32.asm
rem ml /coff /c crypto\des\asm\d-win32.asm
rem ml /coff /c crypto\bn\asm\x86nt32.asm

cl /Focrypto.obj -DWIN32 %OPTIONS% -c crypto\crypto.c
cl /Fossl.obj -DWIN32 %OPTIONS% -c ssl\ssl.c
cl /Foeay.obj -DWIN32 %OPTIONS% -c apps\eay.c

cl /Fessleay.exe %OPTIONS% eay.obj ssl.obj crypto.obj crypto\bf\asm\b-win32.obj crypto\des\asm\c-win32.obj crypto\des\asm\d-win32.obj crypto\bn\asm\x86nt32.obj user32.lib gdi32.lib ws2_32.lib

