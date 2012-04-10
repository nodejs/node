$! TRSA.COM  --  Tests rsa keys
$
$	__arch = "VAX"
$	if f$getsyi("cpu") .ge. 128 then -
	   __arch = f$edit( f$getsyi( "ARCH_NAME"), "UPCASE")
$	if __arch .eqs. "" then __arch = "UNK"
$!
$	if (p2 .eqs. "64") then __arch = __arch+ "_64"
$!
$	exe_dir = "sys$disk:[-.''__arch'.exe.apps]"
$
$	set noon
$	define/user sys$output nla0:
$	mcr 'exe_dir'openssl no-rsa
$	save_severity=$SEVERITY
$	set on
$	if save_severity
$	then
$	    write sys$output "skipping RSA conversion test"
$	    exit
$	endif
$
$	cmd = "mcr ''exe_dir'openssl rsa"
$
$	t = "testrsa.pem"
$	if p1 .nes. "" then t = p1
$
$	write sys$output "testing RSA conversions"
$	if f$search("fff.*") .nes "" then delete fff.*;*
$	if f$search("ff.*") .nes "" then delete ff.*;*
$	if f$search("f.*") .nes "" then delete f.*;*
$	convert/fdl=sys$input: 't' fff.p
RECORD
	FORMAT STREAM_LF
$
$	write sys$output "p -> d"
$	'cmd' -in fff.p -inform p -outform d -out f.d
$	if $severity .ne. 1 then exit 3
$!	write sys$output "p -> t"
$!	'cmd' -in fff.p -inform p -outform t -out f.t
$!	if $severity .ne. 1 then exit 3
$	write sys$output "p -> p"
$	'cmd' -in fff.p -inform p -outform p -out f.p
$	if $severity .ne. 1 then exit 3
$
$	write sys$output "d -> d"
$	'cmd' -in f.d -inform d -outform d -out ff.d1
$	if $severity .ne. 1 then exit 3
$!	write sys$output "t -> d"
$!	'cmd' -in f.t -inform t -outform d -out ff.d2
$!	if $severity .ne. 1 then exit 3
$	write sys$output "p -> d"
$	'cmd' -in f.p -inform p -outform d -out ff.d3
$	if $severity .ne. 1 then exit 3
$
$!	write sys$output "d -> t"
$!	'cmd' -in f.d -inform d -outform t -out ff.t1
$!	if $severity .ne. 1 then exit 3
$!	write sys$output "t -> t"
$!	'cmd' -in f.t -inform t -outform t -out ff.t2
$!	if $severity .ne. 1 then exit 3
$!	write sys$output "p -> t"
$!	'cmd' -in f.p -inform p -outform t -out ff.t3
$!	if $severity .ne. 1 then exit 3
$
$	write sys$output "d -> p"
$	'cmd' -in f.d -inform d -outform p -out ff.p1
$	if $severity .ne. 1 then exit 3
$!	write sys$output "t -> p"
$!	'cmd' -in f.t -inform t -outform p -out ff.p2
$!	if $severity .ne. 1 then exit 3
$	write sys$output "p -> p"
$	'cmd' -in f.p -inform p -outform p -out ff.p3
$	if $severity .ne. 1 then exit 3
$
$	backup/compare fff.p f.p
$	if $severity .ne. 1 then exit 3
$	backup/compare fff.p ff.p1
$	if $severity .ne. 1 then exit 3
$!	backup/compare fff.p ff.p2
$!	if $severity .ne. 1 then exit 3
$	backup/compare fff.p ff.p3
$	if $severity .ne. 1 then exit 3
$
$!	backup/compare f.t ff.t1
$!	if $severity .ne. 1 then exit 3
$!	backup/compare f.t ff.t2
$!	if $severity .ne. 1 then exit 3
$!	backup/compare f.t ff.t3
$!	if $severity .ne. 1 then exit 3
$
$	backup/compare f.p ff.p1
$	if $severity .ne. 1 then exit 3
$!	backup/compare f.p ff.p2
$!	if $severity .ne. 1 then exit 3
$	backup/compare f.p ff.p3
$	if $severity .ne. 1 then exit 3
$
$	delete f.*;*,ff.*;*,fff.*;*
