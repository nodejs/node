rem win32 dll build

set OPTIONS1=-DDES_ASM -DBN_ASM -DBF_ASM -DFLAT_INC -Iout -Itmp -DL_ENDIAN
set OPTIONS2=/W3 /WX /Ox /Gf /nologo

set OPTIONS=%OPTIONS1% %OPTIONS2%

cl /Felibeay32.dll /GD /MD /LD -DWIN32 %OPTIONS% ms\libeay32.def crypto\crypto.c crypto\bf\asm\b-win32.obj crypto\des\asm\c-win32.obj crypto\des\asm\d-win32.obj crypto\bn\asm\x86nt32.obj user32.lib gdi32.lib ws2_32.lib

cl /Fessleay32.dll /GD /MD /LD -DWIN32 %OPTIONS% ms\ssleay32.def ssl\ssl.c libeay32.lib

cl /Fessleay.exe /MD -DWIN32 %OPTIONS% apps\eay.c ssleay32.lib libeay32.lib user32.lib ws2_32.lib

