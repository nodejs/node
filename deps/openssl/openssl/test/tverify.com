$! TVERIFY.COM
$
$	__arch = "VAX"
$	if f$getsyi("cpu") .ge. 128 then -
	   __arch = f$edit( f$getsyi( "ARCH_NAME"), "UPCASE")
$	if __arch .eqs. "" then __arch = "UNK"
$!
$	if (p1 .eqs. "64") then __arch = __arch+ "_64"
$!
$	line_max = 255 ! Could be longer on modern non-VAX.
$	temp_file_name = "certs_"+ f$getjpi( "", "PID")+ ".tmp"
$	exe_dir = "sys$disk:[-.''__arch'.exe.apps]"
$	cmd = "mcr ''exe_dir'openssl verify ""-CAfile"" ''temp_file_name'"
$	cmd_len = f$length( cmd)
$	pems = "[-.certs...]*.pem"
$!
$!	Concatenate all the certificate files.
$!
$	copy /concatenate 'pems' 'temp_file_name'
$!
$!	Loop through all the certificate files.
$!
$	args = ""
$	old_f = ""
$ loop_file: 
$	    f = f$search( pems)
$	    if ((f .nes. "") .and. (f .nes. old_f))
$	    then
$	      old_f = f
$!
$!	      If this file name would over-extend the command line, then
$!	      run the command now.
$!
$	      if (cmd_len+ f$length( args)+ 1+ f$length( f) .gt. line_max)
$	      then
$	         if (args .eqs. "") then goto disaster
$	         'cmd''args'
$	         args = ""
$	      endif
$!	      Add the next file to the argument list.
$	      args = args+ " "+ f
$	   else
$!            No more files in the list
$	      goto loop_file_end
$	   endif
$	goto loop_file
$	loop_file_end:
$!
$!	Run the command for any left-over arguments.
$!
$	if (args .nes. "")
$	then
$	   'cmd''args'
$	endif
$!
$!	Delete the temporary file.
$!
$	if (f$search( "''temp_file_name';*") .nes. "") then -
	 delete 'temp_file_name';*
$!
$	exit
$!
$	disaster:
$	write sys$output "   Command line too long.  Doomed."
$!
