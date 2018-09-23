$!
$! Delete various test results files.
$!
$ def_orig = f$environment( "default")
$ proc = f$environment( "procedure")
$ proc_dev_dir = f$parse( "A.;", proc) - "A.;"
$!
$ on control_c then goto tidy
$ on error then goto tidy
$!
$ set default 'proc_dev_dir'
$!
$ files := *.cms;*, *.srl;*, *.ss;*, -
   cms.err;*, cms.out;*, newreq.pem;*, -
   p.txt-zlib-cipher;*, -
   smtst.txt;*, testkey.pem;*, testreq.pem;*, -
   test_*.err;*, test_*.out;*, -
   .rnd;*
$!
$ delim = ","
$ i = 0
$ loop:
$    file = f$edit( f$element( i, delim, files), "trim")
$    if (file .eqs. delim) then goto loop_end
$    if (f$search( file) .nes. "") then -
      delete 'p1' 'file'
$    i = i+ 1
$ goto loop
$ loop_end:
$!
$ tidy:
$ 
$ if (f$type( def_orig) .nes. "") then -
   set default 'def_orig'
$!
