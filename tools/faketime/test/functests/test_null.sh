# check that the date doesn't happen to be 0.

run()
{
	run_testcase nulltest
}

nulltest()
{
	typeset tdate=${I2DATES[0]}

	assertneq 0 "$(date +%s)" "($tdate)"
}
