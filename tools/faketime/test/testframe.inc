# framework common functions for use in test suites and test cases

#
# run a test and keep stats on success/failure.
# arguments: a command, possibly a shell function.
# return value: 0 on success, 1 on failure.
# side effects: increments global var NSUCC on success, NFAIL on failure.
#
run_testcase()
{
	if "$@"; then
		((NSUCC++))
		return 0
	else
		((NFAIL++))
		return 1
	fi
}

#
# verbosely check that the test output matches the expected value.
# arguments: the test output, the expected value, and a description.
# return value: 0 on if test output equals expected value; 1 otherwise.
# side effects: prints a descriptive message.
#
asserteq()
{
	typeset out="$1" expected="$2" desc="$3"
	echo -n "out=$out $desc"
	if [ "$out" = "$expected" ]; then
		echo " - ok"
		return 0
	else
		echo " expected=$expected - bad"
		return 1
	fi
}

#
# verbosely check that the test output doesn't match the reference value.
# return value: 1 on if test output equals expected value; 0 if not equal.
# side effects: prints descriptive message.
#
assertneq()
{
	typeset out="$1" ref="$2" desc="$3"
	echo -n "out=$out $desc"
	if [ "$out" = "$ref" ]; then
		echo " ref=$ref - bad"
		return 1
	else
		echo " ref=$ref - ok"
		return 0
	fi
}
