
perl util\mkfiles.pl >MINFO
perl ms\uplink-ia64.pl > ms\uptable.asm
ias -o ms\uptable.obj ms\uptable.asm
perl util\mk1mf.pl VC-WIN64I >ms\nt.mak
perl util\mk1mf.pl dll VC-WIN64I >ms\ntdll.mak

perl util\mkdef.pl 32 libeay > ms\libeay32.def
perl util\mkdef.pl 32 ssleay > ms\ssleay32.def
