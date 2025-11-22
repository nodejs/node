# General TZDB implementation notes

Below are some logs of the logic currently at play to find the local
time records based off user provided nanoseconds (seconds).

Important to note, that currently the logs only exist for notably +/-
time zones that observe DST.

Further testing still needs to be done for time zones without any DST
transition / potentially historically abnormal edge cases.

## Slim format testing

`jiff_tzdb` and potentially others use a different compiled `tzif`
than operating systems.

Where operating systems use the "-b fat", smaller embedded tzifs may
use "-b slim" (it's also worth noting that slim is the default setting)

What does slim actually do? The slim formats "slims" the size of a tzif
by calculating the transition times for a smaller range. Instead of calculating
the transition times in a larger range, the tzif (and thus user) differs to
the POSIX tz string.

So in order to support "slim" format `tzif`s, we need to be able to resolve the
[POSIX tz string](glibc-posix-docs).

### Running tests / logging

While using `jiff_tzdb`, the binary search will run the below:

Running a date from 2017 using jiff:

time array length: 175
Returned idx: Err(175)

This will panic, because we have gone past the supported transition times, so
we should default to parsing the POSIX tz string.


[glibc-posix-docs]:https://sourceware.org/glibc/manual/2.40/html_node/Proleptic-TZ.html
