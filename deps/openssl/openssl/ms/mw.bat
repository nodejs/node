@rem OpenSSL with Mingw32
@rem --------------------

@rem Makefile
perl util\mkfiles.pl >MINFO
perl util\mk1mf.pl Mingw32 >ms\mingw32.mak
@rem DLL definition files
perl util\mkdef.pl 32 libeay >ms\libeay32.def
if errorlevel 1 goto end
perl util\mkdef.pl 32 ssleay >ms\ssleay32.def
if errorlevel 1 goto end

@rem Build the libraries
make -f ms/mingw32.mak
if errorlevel 1 goto end

@rem Generate the DLLs and input libraries
dllwrap --dllname libeay32.dll --output-lib out/libeay32.a --def ms/libeay32.def out/libcrypto.a -lwsock32 -lgdi32
if errorlevel 1 goto end
dllwrap --dllname libssl32.dll --output-lib out/libssl32.a --def ms/ssleay32.def out/libssl.a out/libeay32.a
if errorlevel 1 goto end

echo Done compiling OpenSSL

:end

