$!
$! Analyze bntest output file.
$!
$! Exit status = 1 (success) if all tests passed,
$!               0 (warning) if any test failed.
$!
$! 2011-02-20 SMS.  Added code to skip "#" comments in the input file.
$!
$! 2010-04-05 SMS.  New.  Based (loosely) on perl code in bntest-vms.sh.
$!
$!                  Expect data like:
$!                        test test_name1
$!                        0
$!                        [...]
$!                        test test_name2
$!                        0
$!                        [...]
$!                        [...]
$!
$!                  Some tests have no following "0" lines.
$!
$ result_file_name = f$edit( p1, "TRIM")
$ if (result_file_name .eqs. "")
$ then
$     result_file_name = "bntest-vms.out"
$ endif
$!
$ fail = 0
$ passed = 0
$ tests = 0
$!
$ on control_c then goto tidy
$ on error then goto tidy
$!
$ open /read result_file 'result_file_name'
$!
$ read_loop:
$     read /end = read_loop_end /error = tidy result_file line
$     t1 = f$element( 0, " ", line)
$!
$!    Skip "#" comment lines.
$     if (f$extract( 0, 1, f$edit( line, "TRIM")) .eqs. "#") then -
       goto read_loop
$!
$     if (t1 .eqs. "test")
$     then
$         passed = passed+ 1
$         tests = tests+ 1
$         fail = 1
$         t2 = f$extract( 5, 1000, line)
$         write sys$output "verify ''t2'"
$     else
$         if (t1 .nes. "0")
$         then
$             write sys$output "Failed! bc: ''line'"
$             passed = passed- fail
$             fail = 0
$         endif
$     endif
$ goto read_loop
$ read_loop_end:
$ write sys$output "''passed'/''tests' tests passed"
$!
$ tidy:
$ if f$trnlnm( "result_file", "LNM$PROCESS_TABLE", , "SUPERVISOR", , "CONFINE")
$ then
$     close result_file
$ endif
$!
$ if ((tests .gt. 0) .and. (tests .eq. passed))
$ then
$    exit 1
$ else
$    exit 0
$ endif
$!
