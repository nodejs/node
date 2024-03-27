@ECHO OFF
MKDIR bin\dll bin\static bin\example bin\include
COPY tests\fullbench.c bin\example\
COPY programs\datagen.c bin\example\
COPY programs\datagen.h bin\example\
COPY programs\util.h bin\example\
COPY programs\platform.h bin\example\
COPY lib\common\mem.h bin\example\
COPY lib\common\zstd_internal.h bin\example\
COPY lib\common\error_private.h bin\example\
COPY lib\common\xxhash.h bin\example\
COPY lib\libzstd.a bin\static\libzstd_static.lib
COPY lib\dll\libzstd.* bin\dll\
COPY lib\dll\example\Makefile bin\example\
COPY lib\dll\example\fullbench-dll.* bin\example\
COPY lib\dll\example\README.md bin\
COPY lib\zstd.h bin\include\
COPY lib\common\zstd_errors.h bin\include\
COPY lib\dictBuilder\zdict.h bin\include\
COPY programs\zstd.exe bin\zstd.exe
