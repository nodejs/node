#!/bin/bash -eu
# Copyright 2025 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").
# You may not use this file except in compliance with the License.
# You can obtain a copy in the file LICENSE in the source distribution
# or at https://www.openssl.org/source/license.html
#
# Script to graph logs produced by REPORT_RWLOCK_CONTENTION vs time
# Usage: ./lock-contention-graph.sh <logfile> <labels>
#
# Parameters:
# logfile - output file from REPORT_RWLOCK_CONTENTION
# labels - optional value to add timestamp markers to contention event edges
#          for correlation with log file timestamps
# Output:
# The displayed graph is plots application run time on the x axis in
# microseconds, with the y axis representing contention events for each
# found thread individually as a unit step function.
#
# i.e. thread 1 toggles between the values 0 and 1, with 0 representing
# no contention, and 1 representing waiting on a lock.  Thread 2 is offset
# on the y axis by 1, toggling between 2 and 3, with the former representing no
# contention, and 3 representing waiting on a lock to become available.
###################################################################

TEMPDIR=$(mktemp -d /tmp/contentiongraph.XXXXXX)
LOGFILE=$1
LABELS=$2

trap "rm -rf $TEMPDIR" EXIT

if [ ! -f $LOGFILE ]
then
    echo "No log file found" > /dev/stderr
    exit 1
fi
LOGFILEBASE=$(basename $LOGFILE)

mkdir -p $TEMPDIR/tids/

#
# Gather all our tids
#
declare -a filelines
declare -a sorted_lines

declare -a attimes
declare -a unblocktimes
declare -a levels

let offset=0

for i in $(cat $LOGFILE | grep "lock blocked" $LOGFILE | awk '{print $12}' | sort | uniq); do
    filelines=()
    sorted_lines=()
    mapfile -t filelines < <(cat $LOGFILE | grep "tid $i")
    IFS=$'\n' sorted_lines=($(sort -k 10 -n <<<"${filelines[*]}"))
    unset IFS

    attimes=()
    unblocktimes=()
    levels=()
    rawtime=()
    let up=$offset+1
    let down=$offset
    let firsttime=0
    echo "Processing tid $i"
    for LINE in "${sorted_lines[@]}"; do
        DURATION=$(echo $LINE | awk '{print $6}')
        ATTIME=$(echo $LINE | awk '{print $10}')
        UNBLOCKTIME=$(dc -e "$ATTIME $DURATION + p")
        if [ $firsttime -eq 0 ]; then
            let firsttime=$ATTIME
        fi
        rawtime+=($ATTIME)
        ATTIME=$(dc -e"$ATTIME $firsttime - p")
        UNBLOCKTIME=$(dc -e "$UNBLOCKTIME $firsttime - p")
        attimes+=($ATTIME)
        levels+=($down)
        levels+=($up)
        unblocktimes+=($UNBLOCKTIME)
        levels+=($up)
        levels+=($down)
    done

#
# Write out our array to a file
#
    NUMELEMS=${#attimes[@]}
    for j in $(seq 0 1 $NUMELEMS); do
        let lvlidx=$j*4
        echo "${attimes[$j]} ${levels[$lvlidx]} ${rawtime[$j]}" >> $TEMPDIR/tids/$i.data
        let lvlidx=$lvlidx+1
        echo "${attimes[$j]} ${levels[$lvlidx]}" >> $TEMPDIR/tids/$i.data
        let lvlidx=$lvlidx+1
        echo "${unblocktimes[$j]} ${levels[$lvlidx]}" >> $TEMPDIR/tids/$i.data
        let lvlidx=$lvlidx+1
        echo "${unblocktimes[$j]} ${levels[$lvlidx]}" >> $TEMPDIR/tids/$i.data
    done

    let offset=$offset+2
done

#
# Now lets use gnuplot to plot all the contentions
#
cat << EOF > $TEMPDIR/gnuplot.script
set term qt 
set format x '%.0f'
set xlabel "usecs"
set ylabel "contentions"
set yrange [0:5]
set xtics 10000
set xrange [0:5000000]
EOF

echo -n "plot " >> $TEMPDIR/gnuplot.script

for i in $(ls $TEMPDIR/tids/*.data)
do
    TITLE=$(basename $i)
    echo -n "\"$i\" using 1:2 with lines title \"tid $TITLE\", " >> $TEMPDIR/gnuplot.script
    if [ -n "$LABELS" ]; then
        echo -n "\"$i\" using 1:2:3 with labels offset 0, char 1 notitle, " >> $TEMPDIR/gnuplot.script
    fi
done

echo "" >> $TEMPDIR/gnuplot.script
echo "pause -1" >> $TEMPDIR/gnuplot.script

gnuplot $TEMPDIR/gnuplot.script

