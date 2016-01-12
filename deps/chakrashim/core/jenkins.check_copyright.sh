#-------------------------------------------------------------------------------------------------------
# Copyright (C) Microsoft. All rights reserved.
# Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
#-------------------------------------------------------------------------------------------------------

# Need to make sure that the reference to origin/master is available.
# We know that HEAD is checked out so that the tests on that source can be run.

# configure the sh environment to run scripts from the bin dir in case that's missing
ls &> /dev/null # checking for ls script on the path
if [ $? -ne 0 ]; then
    PATH=/bin:/usr/bin:$PATH
fi

ERRFILE=jenkins.check_copyright.sh.err
ERRFILETEMP=$ERRFILE.0
rm -f $ERRFILE
rm -f $ERRFILETEMP

echo "Check Copyright > Begin Checking..."
git diff --name-only `git merge-base origin/master HEAD` HEAD |
    grep -v -E '\.git.*' |
    grep -v -E '\.xml$' |
    grep -v -E '\.props$' |
    grep -v -E '\.md$' |
    grep -v -E '\.txt$' |
    grep -v -E '\.baseline$' |
    grep -v -E '\.sln$' |
    grep -v -E '\.vcxproj$' |
    grep -v -E '\.filters$' |
    grep -v -E '\.targets$' |
    grep -v -E '\.def$' |
    grep -v -E '\.inc$' |
    grep -v -E 'test/benchmarks/.*\.js$' |
    xargs -I % sh -c "echo 'Check Copyright > Checking %'; python jenkins.check_copyright.py % > $ERRFILETEMP; if [ \$? -ne 0 ]; then cat $ERRFILETEMP >> $ERRFILE; fi"

if [ -e $ERRFILE ]; then # if error file exists then there were errors
    >&2 echo "--------------" # leading >&2 means echo to stderr
    >&2 echo "--- ERRORS ---"
    cat $ERRFILE 1>&2 # send output to stderr so it can be redirected as error if desired
    >&2 echo "--------------"
    exit 1 # tell the caller there was an error (so Jenkins will fail the CI task)
else
    echo "--- NO PROBLEMS DETECTED ---"
fi
