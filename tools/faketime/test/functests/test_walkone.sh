# walking-1 test.
# sourced in from testframe.sh.
#
# this script defines a suite of functional tests
# that verifies the correct operation of libfaketime
# with the date command.

run()
{
	init

	for i in $(range 0 30); do
		run_testcase test_with_i $i
	done
}

# ----- support routines
init()
{
	typeset testsuite="$1"
	PLATFORM=$(platform)
	if [ -z "$PLATFORM" ]; then
		echo "$testsuite: unknown platform! quitting"
		return 1
	fi
	echo "# PLATFORM=$PLATFORM"
	return 0
}


# run date cmd under faketime, print time in secs
fakedate()
{
	#
	# let the time format be raw seconds since Epoch
	# for both input to libfaketime, and output of the date cmd.
	#
	typeset fmt='%s'
	export FAKETIME_FMT=$fmt
	fakecmd "$1" date +$fmt
}

#
# compute x**n.
# use only the shell, in case we need to run on machines
# without bc, dc, perl, etc.
#
pow()
{
	typeset x="$1" n="$2"
	typeset r=1
	typeset i=0
	while ((i < n)); do
		((r = r*x))
		((i++))
	done
	echo $r
}

# run a fakedate test with a given time t
test_with_i()
{
	typeset i="$1"
	typeset t=$(pow 2 $i)

	asserteq $(fakedate $t) $t "(secs since Epoch)"
}
