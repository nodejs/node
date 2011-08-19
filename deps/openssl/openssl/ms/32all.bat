set OPTS=no-asm

perl Configure VC-WIN32
perl util\mkfiles.pl >MINFO
perl util\mk1mf.pl %OPTS% debug VC-WIN32 >d32.mak
perl util\mk1mf.pl %OPTS% VC-WIN32 >32.mak
perl util\mk1mf.pl %OPTS% debug dll VC-WIN32 >d32dll.mak
perl util\mk1mf.pl %OPTS% dll VC-WIN32 >32dll.mak
perl util\mkdef.pl 32 libeay > ms\libeay32.def
perl util\mkdef.pl 32 ssleay > ms\ssleay32.def

nmake -f d32.mak
@if errorlevel 1 goto end
nmake -f 32.mak
@if errorlevel 1 goto end
nmake -f d32dll.mak
@if errorlevel 1 goto end
nmake -f 32dll.mak

:end
