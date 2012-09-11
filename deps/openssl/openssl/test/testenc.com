$! TESTENC.COM  --  Test encoding and decoding
$
$	__arch = "VAX"
$	if f$getsyi("cpu") .ge. 128 then -
	   __arch = f$edit( f$getsyi( "ARCH_NAME"), "UPCASE")
$	if __arch .eqs. "" then __arch = "UNK"
$!
$	if (p1 .eqs. 64) then __arch = __arch+ "_64"
$
$	exe_dir = "sys$disk:[-.''__arch'.exe.apps]"
$	testsrc = "makefile."
$	test = "p.txt"
$	cmd = "mcr ''exe_dir'openssl"
$
$	if f$search(test) .nes. "" then delete 'test';*
$	convert/fdl=sys$input: 'testsrc' 'test'
RECORD
	FORMAT STREAM_LF
$
$	if f$search(test+"-cipher") .nes. "" then delete 'test'-cipher;*
$	if f$search(test+"-clear") .nes. "" then delete 'test'-clear;*
$
$	write sys$output "cat"
$	'cmd' enc -in 'test' -out 'test'-cipher
$	'cmd' enc -in 'test'-cipher -out 'test'-clear
$	backup/compare 'test' 'test'-clear
$	if $severity .ne. 1 then exit 3
$	delete 'test'-cipher;*,'test'-clear;*
$
$	write sys$output "base64"
$	'cmd' enc -a -e -in 'test' -out 'test'-cipher
$	'cmd' enc -a -d -in 'test'-cipher -out 'test'-clear
$	backup/compare 'test' 'test'-clear
$	if $severity .ne. 1 then exit 3
$	delete 'test'-cipher;*,'test'-clear;*
$
$	define/user sys$output 'test'-cipher-commands
$	'cmd' list-cipher-commands
$	open/read f 'test'-cipher-commands
$ loop_cipher_commands:
$	read/end=loop_cipher_commands_end f i
$	write sys$output i
$
$	if f$search(test+"-"+i+"-cipher") .nes. "" then -
		delete 'test'-'i'-cipher;*
$	if f$search(test+"-"+i+"-clear") .nes. "" then -
		delete 'test'-'i'-clear;*
$
$	'cmd' 'i' -bufsize 113 -e -k test -in 'test' -out 'test'-'i'-cipher
$	'cmd' 'i' -bufsize 157 -d -k test -in 'test'-'i'-cipher -out 'test'-'i'-clear
$	backup/compare 'test' 'test'-'i'-clear
$	if $severity .ne. 1 then exit 3
$	delete 'test'-'i'-cipher;*,'test'-'i'-clear;*
$
$	write sys$output i," base64"
$	'cmd' 'i' -bufsize 113 -a -e -k test -in 'test' -out 'test'-'i'-cipher
$	'cmd' 'i' -bufsize 157 -a -d -k test -in 'test'-'i'-cipher -out 'test'-'i'-clear
$	backup/compare 'test' 'test'-'i'-clear
$	if $severity .ne. 1 then exit 3
$	delete 'test'-'i'-cipher;*,'test'-'i'-clear;*
$
$	goto loop_cipher_commands
$ loop_cipher_commands_end:
$	close f
$	delete 'test'-cipher-commands;*
$	delete 'test';*
