#!/bin/sh

# A wrapper script to call 'linux-tick-processor'.

# Known issues on FreeBSD:
#  No ticks from C++ code.
#  You must have d8 built and in your path before calling this.

tools_path=`cd $(dirname "$0");pwd`
$tools_path/linux-tick-processor "$@"
