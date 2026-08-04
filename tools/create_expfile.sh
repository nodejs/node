#!/bin/sh
# Writes out all of the exported symbols to a file.
# This is needed on AIX as symbols are not exported
# by an executable by default and need to be listed
# specifically for export so that they can be used
# by native add-ons.
#
# The raw symbol data is obtained by using dump -tov on
# the .a files which make up the node executable.
#
# dump -tov flags:
# -t: Display symbol table entries
# -o: Display object file headers
# -v: Display visibility information (HIDDEN/EXPORTED/PROTECTED)
# -X 32_64: Process both 32-bit and 64-bit symbols
#
# This approach is similar to CMake's ExportImportList module for AIX:
# https://github.com/Kitware/CMake/blob/45f4742cbfe6c25c7c801e3806c8af04a2c4b09b/Modules/Platform/AIX/ExportImportList#L67-L80
#
# We filter for symbols in .text, .data, .tdata, and .bss sections
# with extern or weak linkage, excluding:
# - Symbols starting with dot (internal/local symbols)
# - __sinit/__sterm (static initialization/termination)
# - __[0-9]+__ (compiler-generated symbols)
# - HIDDEN visibility symbols (to avoid linker error 0711-407)
#
# The final sort allows removal of any duplicates.
#
# Symbols for the gtest libraries are excluded as
# they are not linked into the node executable.
#
echo "Searching $1 to write out expfile to $2"

# This special sequence must be at the start of the exp file.
echo "#!." > "$2.tmp"

# Pull the symbols from the .a files.
# Use dump -tov to get visibility information and exclude HIDDEN symbols.
# This prevents AIX linker error 0711-407 when addons try to import symbols
# with visibility attributes.
#
# IMPORTANT: AIX dump -tov output has a visibility column that contains either
# a visibility keyword (HIDDEN/EXPORTED/PROTECTED) or blank spaces when no
# visibility attribute is set. Since AWK splits fields on whitespace, this
# results in different field counts:
#
# With visibility keyword (8 fields after AWK splits):
#   [137] m 0x00003290 .text 1 weak HIDDEN ._ZNKSt3__1...
#    $1  $2    $3      $4   $5  $6    $7        $8
#                              |     |         └─ Symbol name
#                              |     └─ Visibility (HIDDEN/EXPORTED/PROTECTED)
#                              └─ Linkage (weak/extern)
#
# Without visibility keyword (7 fields after AWK splits, spaces collapsed):
#   [77] m 0x00000000 .text 1 weak ._ZN8simdjson...
#    $1 $2    $3      $4   $5  $6       $7
#                             |         └─ Symbol name
#                             └─ Linkage (weak/extern)
#
# The awk script handles both cases by using an associative array V[]
# to map field values to their export suffixes. For fields that don't match
# any key in V[], the lookup returns an empty string, which is perfect for
# handling the variable field positions caused by the blank visibility column.
#
find "$1" -name "*.a" | grep -v gtest | while read -r f; do
    dump -tov -X 32_64 "$f" 2>/dev/null | \
    awk '
        BEGIN {
            V["EXPORTED"]=" export"
            V["PROTECTED"]=" protected"
            V["HIDDEN"]=" hidden"
            V["weak"]=" weak"
        }
        /^\[[0-9]+\]\tm +[^ ]+ +\.(text|data|tdata|bss) +[^ ]+ +(extern|weak)( +(EXPORTED|PROTECTED|HIDDEN))?/ {
            # Exclude symbols starting with dot, __sinit, __sterm, __[0-9]+__
            # Also exclude HIDDEN symbols to avoid visibility attribute issues
            if (!match($NF,/^(\.|__sinit|__sterm|__[0-9]+__)/)) {
                # Skip if HIDDEN visibility (can be at $(NF-1) or $(NF-2))
                if ($(NF-1) != "HIDDEN" && $(NF-2) != "HIDDEN") {
                    print $NF V[$(NF-1)] V[$(NF-2)]
                }
            }
        }
    '
done | sort -u >> "$2.tmp"

mv -f "$2.tmp" "$2"
