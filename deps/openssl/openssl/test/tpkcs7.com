$! TPKCS7.COM  --  Tests pkcs7 keys
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
$	cmd = "mcr ''exe_dir'openssl pkcs7"
$
$	t = "testp7.pem"
$	if p1 .nes. "" then t = p1
$
$	write sys$output "testing PKCS7 conversions"
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
$	write sys$output "p -> p"
$	'cmd' -in fff.p -inform p -outform p -out f.p
$	if $severity .ne. 1 then exit 3
$
$	write sys$output "d -> d"
$	'cmd' -in f.d -inform d -outform d -out ff.d1
$	if $severity .ne. 1 then exit 3
$	write sys$output "p -> d"
$	'cmd' -in f.p -inform p -outform d -out ff.d3
$	if $severity .ne. 1 then exit 3
$
$
$	write sys$output "d -> p"
$	'cmd' -in f.d -inform d -outform p -out ff.p1
$	if $severity .ne. 1 then exit 3
$	write sys$output "p -> p"
$	'cmd' -in f.p -inform p -outform p -out ff.p3
$	if $severity .ne. 1 then exit 3
$
$	backup/compare fff.p f.p
$	if $severity .ne. 1 then exit 3
$	backup/compare fff.p ff.p1
$	if $severity .ne. 1 then exit 3
$	backup/compare fff.p ff.p3
$	if $severity .ne. 1 then exit 3
$
$	backup/compare f.p ff.p1
$	if $severity .ne. 1 then exit 3
$	backup/compare f.p ff.p3
$	if $severity .ne. 1 then exit 3
$
$	delete f.*;*,ff.*;*,fff.*;*
