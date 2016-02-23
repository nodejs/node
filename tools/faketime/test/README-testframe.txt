# here's how the testframe script works.
#
# Usage for testing:
#	usage: testframe.sh DIR
# testframe.sh runs each testsuite script found within DIR.
# (in the context of libfaketime, the DIR is functest.)
# exits with status 0 if all tests succeed.
#
# Interface:
# by convention, each testsuite script (within DIR) must be
# a bash script named test_*.sh.  the script must define a
# function named "run".  run takes no arguments.  run is
# expected to call the framework-provided function
# run_testcase once for each test function.  run_testcase
# uses the global vars NFAIL and NSUCC to keep track of how
# many testcases failed/succeeded.
#
# the test function is expected to call something like
# asserteq or assertneq (again, framework-provided).
#
# fine print: for each testsuite, the framework creates a
# subshell and dots in the script.  also dotted in are
# testframe.inc and DIR/common.inc (if it exists).  the
# testsuite script can make use of any functions defined
# in these inc files.  the environment variable
# TESTSUITE_NAME is set to the filename of the testsuite
# script, for possible use in warning or info messages.
#
# see functests/test_true.sh for a simple example of
# a test suite script.
#
# Simple steps to add a new testsuite:
# 1. decide its name - eg, XXX.
# 2. choose a DIR of similar testsuites to put it in, or create a new one.
# 3. create DIR/test_XXX.sh.
# 4. write a run function and testcase functions in DIR/test_XXX.sh.
# 5. within the run function, call run_testcase for each testcase function.
# 6. within each testcase funtion, call assertneq or asserteq, or do
#    the equivalent.
