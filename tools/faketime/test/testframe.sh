#! /bin/bash
#	testframe.sh DIR
# bare-bones testing framework.
# run the test suites in the given DIR;
# exit with nonzero status if any of them failed.
# see README.testframe.txt for details.
#

# echo labelled error/warning message to stderr
report()
{
	echo $PROG: $* 1>&2
}

# echo OK or BAD depending on argument (0 or not)
status_word()
{
	if [ "$1" -eq 0 ]; then
		echo OK
	else
		echo BAD
	fi
}

# run the given testsuite, return nonzero if any testcase failed.
run_testsuite()
{
	typeset testsuite="$1"

	NFAIL=0
	NSUCC=0

	# add testsuite dir to PATH for convenience
	typeset dir=$(dirname $testsuite)
	PATH=$dir:$PATH
	. testframe.inc
	if [ -f $dir/common.inc ]; then
		. $dir/common.inc
	fi
	. $testsuite
	export TESTSUITE_NAME=$testsuite

	echo ""
	echo "# Begin $testsuite"

	run
	typeset runstat=$?

	echo "# $testsuite summary: $NSUCC succeeded, $NFAIL failed"
	if [ $runstat -ne 0 ]; then
		((NFAIL++))
		report "error: $testsuite run exit_status=$runstat!"
	fi
	echo "# End $testsuite -" $(status_word $NFAIL)
	[ $NFAIL -eq 0 ]
}

#
# list all testsuite scripts in the given directories.
# a testsuite file must be a bash script whose name is of the form test_*.sh .
#
list_testsuites()
{
	for dir in "$@"; do
		ls $dir/test_*.sh 2>/dev/null
	done
}

main()
{
	TS_NFAIL=0
	TS_NSUCC=0

	echo "# Begin Test Suites in $*"
	typeset testsuites=$(list_testsuites "$@")

	if [ -z "$testsuites" ]; then
		report "error: no testsuites found"
		exit 1
	fi

	for testsuite in $testsuites; do
		if run_testsuite $testsuite; then
			((TS_NSUCC++))
		else
			((TS_NFAIL++))
		fi
	done

	echo ""
	echo "# Test Suites summary: $TS_NSUCC succeeded, $TS_NFAIL failed"
	echo "# End Test Suites -" $(status_word $TS_NFAIL)
	[ $TS_NFAIL -eq 0 ]
}

# ----- start of mainline code
PROG=${0##*/}

main "${@:-.}"
