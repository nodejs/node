perl Configure BC-32
perl util\mkfiles.pl > MINFO

@rem create make file
perl util\mk1mf.pl no-asm BC-NT > bcb.mak

