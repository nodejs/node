$ if f$search("ioapi.h_orig") .eqs. "" then copy ioapi.h ioapi.h_orig
$ open/write zdef vmsdefs.h
$ copy sys$input: zdef
$ deck
#define unix
#define fill_zlib_filefunc64_32_def_from_filefunc32 fillzffunc64from
#define Write_Zip64EndOfCentralDirectoryLocator Write_Zip64EoDLocator
#define Write_Zip64EndOfCentralDirectoryRecord Write_Zip64EoDRecord
#define Write_EndOfCentralDirectoryRecord Write_EoDRecord
$ eod
$ close zdef
$ copy vmsdefs.h,ioapi.h_orig ioapi.h
$ cc/include=[--]/prefix=all ioapi.c
$ cc/include=[--]/prefix=all miniunz.c
$ cc/include=[--]/prefix=all unzip.c
$ cc/include=[--]/prefix=all minizip.c
$ cc/include=[--]/prefix=all zip.c
$ link miniunz,unzip,ioapi,[--]libz.olb/lib
$ link minizip,zip,ioapi,[--]libz.olb/lib
$ mcr []minizip test minizip_info.txt
$ mcr []miniunz -l test.zip
$ rename minizip_info.txt; minizip_info.txt_old
$ mcr []miniunz test.zip
$ delete test.zip;*
$exit
