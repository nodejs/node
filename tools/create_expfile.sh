#!/bin/sh
# This script writes out all the exported symbols to a file
# AIX needs this as sybmols are not exported by an
# executable by default and we need to list
# them specifically in order to export them
# so that they can be used by native add-ons
#
# The raw symbol data is objtained by using nm on
# the .a files which make up the node executable
#
# -Xany makes sure we get symbols on both
# 32 bit and 64 bit as by default we'd only get those
# for 32 bit
#
# -g selects only exported symbols
#
# -C, -B and -p ensure the output is in a format we
# can easily parse and convert into the symbol we need
#
# -C suppresses the demangling of C++ names
# -B gives us output in BSD format
# -p displays the info in a standard portable output format
#
# We only include symbols if they are of the
# following types and don't start with a dot.
#
# T - Global text symbol
# D - Global data symbol
# B - Gobal bss symbol.
#
# the final sort allows us to remove any duplicates
#
# We need to exclude gtest libraries as they are not
# linked into the node executable
#
echo "Searching $1 to write out expfile to $2"

# this special sequence must be at the start of the exp file
echo "#!." > $2

# pull the symbols from the .a files
find $1 -name "*.a" | grep -v gtest \
  | xargs nm -Xany -BCpg \
  | awk '{
      if ((($2 == "T") || ($2 == "D") || ($2 == "B")) &&
          (substr($3,1,1) != ".")) { print $3 }
    }' \
  | sort -u >> $2
