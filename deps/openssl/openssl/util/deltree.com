$! DELTREE.COM
$
$ call deltree 'p1'
$ exit $status
$
$ deltree: subroutine ! P1 is a name of a directory
$	on control_y then goto dt_STOP
$	on warning then goto dt_exit
$	_dt_def = f$trnlnm("SYS$DISK")+f$directory()
$	if f$parse(p1) .eqs. "" then exit
$	set default 'f$parse(p1,,,"DEVICE")''f$parse(p1,,,"DIRECTORY")'
$	p1 = f$parse(p1,,,"NAME") + f$parse(p1,,,"TYPE")
$	_fp = f$parse(".DIR",p1)
$ dt_loop:
$	_f = f$search(_fp)
$	if _f .eqs. "" then goto dt_loopend
$	call deltree [.'f$parse(_f,,,"NAME")']*.*
$	goto dt_loop
$ dt_loopend:
$	_fp = f$parse(p1,".;*")
$	if f$search(_fp) .eqs. "" then goto dt_exit
$	set noon
$	set file/prot=(S:RWED,O:RWED,G:RWED,W:RWED) '_fp'
$	set on
$	delete/nolog '_fp'
$ dt_exit:
$	set default '_dt_def'
$	goto dt_end
$ dt_STOP:
$	set default '_dt_def'
$	stop/id=""
$	exit
$ dt_end:
$	endsubroutine
