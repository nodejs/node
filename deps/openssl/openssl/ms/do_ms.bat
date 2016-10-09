
perl util\mkfiles.pl >MINFO
perl util\mk1mf.pl no-asm VC-WIN32 >ms\nt.mak
perl util\mk1mf.pl dll no-asm VC-WIN32 >ms\ntdll.mak
if x%OSVERSION% == x goto skipce
perl util\mk1mf.pl no-asm VC-CE >ms\ce.mak
perl util\mk1mf.pl dll no-asm VC-CE >ms\cedll.mak
:skipce

perl util\mkdef.pl 32 libeay > ms\libeay32.def
perl util\mkdef.pl 32 ssleay > ms\ssleay32.def
