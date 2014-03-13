#!/bin/bash
#
# Copyright 2013 the V8 project authors. All rights reserved.
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
#       copyright notice, this list of conditions and the following
#       disclaimer in the documentation and/or other materials provided
#       with the distribution.
#     * Neither the name of Google Inc. nor the names of its
#       contributors may be used to endorse or promote products derived
#       from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# This script reads in CSV formatted instruction data, and draws a stacked
# graph in png format.

defaultfile=a64_inst.csv
defaultout=a64_inst.png
gnuplot=/usr/bin/gnuplot


# File containing CSV instruction data from simulator.
file=${1:-$defaultfile}

# Output graph png file.
out=${2:-$defaultout}

# Check input file exists.
if [ ! -e $file ]; then
  echo "Input file not found: $file."
  echo "Usage: draw_instruction_graph.sh <input csv> <output png>"
  exit 1
fi

# Search for an error message, and if found, exit.
error=`grep -m1 '# Error:' $file`
if [ -n "$error" ]; then
  echo "Error message in input file:"
  echo " $error"
  exit 2
fi

# Sample period - period over which numbers for each category of instructions is
# counted.
sp=`grep -m1 '# sample_period=' $file | cut -d= -f2`

# Get number of counters in the CSV file.
nc=`grep -m1 '# counters=' $file | cut -d= -f2`

# Find the annotation arrows. They appear as comments in the CSV file, in the
# format:
#   # xx @ yyyyy
# Where xx is a two character annotation identifier, and yyyyy is the
# position in the executed instruction stream that generated the annotation.
# Turn these locations into labelled arrows.
arrows=`sed '/^[^#]/ d' $file | \
        perl -pe "s/^# .. @ (\d+)/set arrow from \1, graph 0.9 to \1, $sp/"`;
labels=`sed '/^[^#]/d' $file | \
        sed -r 's/^# (..) @ (.+)/set label at \2, graph 0.9 "\1" \
                center offset 0,0.5 font "FreeSans, 8"/'`;

# Check for gnuplot, and warn if not available.
if [ ! -e $gnuplot ]; then
  echo "Can't find gnuplot at $gnuplot."
  echo "Gnuplot version 4.6.3 or later required."
  exit 3
fi

# Initialise gnuplot, and give it the data to draw.
echo | $gnuplot <<EOF
$arrows
$labels
MAXCOL=$nc
set term png size 1920, 800 #ffffff
set output '$out'
set datafile separator ','
set xtics font 'FreeSans, 10'
set xlabel 'Instructions' font 'FreeSans, 10'
set ytics font 'FreeSans, 10'
set yrange [0:*]
set key outside font 'FreeSans, 8'

set style line 2 lc rgb '#800000'
set style line 3 lc rgb '#d00000'
set style line 4 lc rgb '#ff6000'
set style line 5 lc rgb '#ffc000'
set style line 6 lc rgb '#ffff00'

set style line 7 lc rgb '#ff00ff'
set style line 8 lc rgb '#ffc0ff'

set style line 9 lc rgb '#004040'
set style line 10 lc rgb '#008080'
set style line 11 lc rgb '#40c0c0'
set style line 12 lc rgb '#c0f0f0'

set style line 13 lc rgb '#004000'
set style line 14 lc rgb '#008000'
set style line 15 lc rgb '#40c040'
set style line 16 lc rgb '#c0f0c0'

set style line 17 lc rgb '#2020f0'
set style line 18 lc rgb '#6060f0'
set style line 19 lc rgb '#a0a0f0'

set style line 20 lc rgb '#000000'
set style line 21 lc rgb '#ffffff'

plot for [i=2:MAXCOL] '$file' using 1:(sum [col=i:MAXCOL] column(col)) \
title columnheader(i) with filledcurve y1=0 ls i
EOF



