# Checks that setting DONT_FAKE_MONOTONIC actually prevent
# libfaketime from faking monotonic clocks.
#
# We do this by freezing time at a specific and arbitrary date with faketime,
# and making sure that if we set DONT_FAKE_MONOTONIC to 1, calling
# clock_gettime(CLOCK_MONOTONIC) returns two different values.
#
# We also make sure that if we don't set DONT_FAKE_MONOTONIC to 1, in other
# words when we use the default behavior, two subsequent calls to
# clock_gettime(CLOCK_MONOTONIC) do return different values.

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

run()
{
    init

    run_testcase dont_fake_mono
    run_testcase fake_mono
}

get_token()
{
    string=$1
    token_index=$2
    separator=$3

    echo $string | cut -d "$separator" -f $token_index
}

assert_timestamps_neq()
{
    timestamps=$1
    msg=$2

    first_timestamp=$(get_token "${timestamps}" 1 ' ')
    second_timestamp=$(get_token "${timestamps}" 2 ' ')

    assertneq "${first_timestamp}" "${second_timestamp}" "${msg}"
}

assert_timestamps_eq()
{
    timestamps=$1
    msg=$2

    first_timestamp=$(get_token "${timestamps}" 1 ' ')
    second_timestamp=$(get_token "${timestamps}" 2 ' ')

    asserteq "${first_timestamp}" "${second_timestamp}" "${msg}"
}

get_monotonic_time()
{
    dont_fake_mono=$1; shift;
    clock_id=$1; shift;
    DONT_FAKE_MONOTONIC=${dont_fake_mono} fakecmd "2014-07-21 09:00:00" \
    /bin/bash -c "for i in 1 2; do \
    perl -w -MTime::HiRes=clock_gettime,${clock_id} -E \
    'say clock_gettime(${clock_id})'; \
    sleep 1; \
    done"
}

dont_fake_mono()
{
    timestamps=$(get_monotonic_time 1 CLOCK_MONOTONIC)
    msg="When not faking monotonic time, timestamps should be different"
    assert_timestamps_neq "${timestamps}" "${msg}"
}

fake_mono()
{
    timestamps=$(get_monotonic_time 0 CLOCK_MONOTONIC)
    msg="When faking monotonic, timestamps should be equal"
    assert_timestamps_eq "${timestamps}" "${msg}"
}
