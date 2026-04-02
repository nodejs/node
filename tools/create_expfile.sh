#!/bin/sh
# Writes out all of the exported symbols to a file.
# This is needed on AIX as symbols are not exported
# by an executable by default and need to be listed
# specifically for export so that they can be used
# by native add-ons.
#
# The raw symbol data is obtained by using nm on
# the .a files which make up the node executable.
#
# -Xany processes symbols for both 32-bit and
# 64-bit (the default is for 32-bit only).
#
# -g selects only exported symbols.
#
# -C, -B and -p ensure that the output is in a
# format that can be easily parsed and converted
# into the required symbol.
#
# -C suppresses the demangling of C++ names.
# -B writes the output in BSD format.
# -p displays the info in a standard portable
#    output format.
#
# Only include symbols if they are of the following
# types and don't start with a dot.
#
# T - Global text symbol.
# D - Global data symbol.
# B - Global bss symbol.
#
# The final sort allows removal of any duplicates.
#
# Symbols for the gtest libraries are excluded as
# they are not linked into the node executable.
#
set -x
echo "Searching $1 to write out expfile to $2"

# This special sequence must be at the start of the exp file.
echo "#!." > "$2.tmp"

# Pull the symbols from the .a files.
# Use dump -tov to get visibility information and exclude HIDDEN symbols
# This prevents AIX linker error 0711-407 when addons try to import symbols
# with visibility attributes.
find "$1" -name "*.a" | grep -v gtest | while read f; do
    dump -tov -X 32_64 "$f" 2>/dev/null | \
    awk '
        BEGIN {
            V["EXPORTED"]=" export"
            V["PROTECTED"]=" protected"
            V["HIDDEN"]=" hidden"
        }
        /^\[[0-9]+\]\tm +[^ ]+ +\.(text|data|tdata|bss) +[^ ]+ +(extern|weak) +(EXPORTED|PROTECTED|HIDDEN| ) / {
            # Exclude symbols starting with dot, __sinit, __sterm, __[0-9]+__
            # Also exclude HIDDEN symbols to avoid visibility attribute issues
            if (!match($NF,/^(\.|__sinit|__sterm|__[0-9]+__)/)) {
                visibility = $(NF-1)
                if (visibility != "HIDDEN") {
                    print $NF
                }
            }
        }
    '
done | sort -u >> "$2.tmp"

mv -f "$2.tmp" "$2"
