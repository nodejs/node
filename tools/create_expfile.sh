#!/bin/sh
# Writes out all of the exported symbols to a file.
# This is needed on AIX as symbols are not exported
# by an executable by default and need to be listed
# specifically for export so that they can be used
# by native add-ons.
#
# The raw symbol data is obtained by using dump -tov on
# the .a and .o files which make up the node executable.
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
SEARCH_DIR="$1"
OUTPUT="$2"
TARGET_NAME="$3"
shift 3 || true
# Remaining arguments are explicit extra inputs for the build to order on.

echo "Searching $SEARCH_DIR to write out expfile to $OUTPUT"

SYMFILE="${OUTPUT}.syms"
: > "$SYMFILE"

# Use dump -tov to get visibility information and exclude HIDDEN symbols.
# This prevents AIX linker error 0711-407 when addons try to import symbols
# with visibility attributes.
#
# IMPORTANT: AIX dump -tov output has a visibility column that contains either
# a visibility keyword (HIDDEN/EXPORTED/PROTECTED) or blank spaces when no
# visibility attribute is set. Since AWK splits fields on whitespace, this
# results in different field counts. The awk script handles both cases by using
# an associative array V[] to map field values to their export suffixes.

# This is to bypass the lint-sh job from saying "Expressions don't expand in
# single quotes, use double quotes for that." which is invalid for this case
# shellcheck disable=SC2016
DUMP_AWK='
BEGIN {
    V["EXPORTED"]=" export"
    V["PROTECTED"]=" protected"
    V["HIDDEN"]=" hidden"
    V["weak"]=" weak"
}
/^\[[0-9]+\]\tm +[^ ]+ +\.(text|data|tdata|bss) +[^ ]+ +(extern|weak)( +(EXPORTED|PROTECTED|HIDDEN))?/ {
    if (!match($NF,/^(\.|__sinit|__sterm|__[0-9]+__)/)) {
        if ($(NF-1) != "HIDDEN" && $(NF-2) != "HIDDEN") {
            print $NF V[$(NF-1)] V[$(NF-2)]
        }
    }
}
'

find "$SEARCH_DIR" -name "*.a" | grep -v gtest | while read -r f; do
  dump -tov -X 32_64 "$f" 2>/dev/null | awk "$DUMP_AWK" >> "$SYMFILE"
done

# Objects linked directly into the target (not archived), e.g. node_snapshot.o.
if [ -n "$TARGET_NAME" ] && [ -d "$SEARCH_DIR/$TARGET_NAME" ]; then
  find "$SEARCH_DIR/$TARGET_NAME" -name "*.o" | while read -r f; do
    dump -tov -X 32_64 "$f" 2>/dev/null | awk "$DUMP_AWK" >> "$SYMFILE"
  done
fi

for extra in "$@"; do
  if [ -f "$extra" ]; then
    dump -tov -X 32_64 "$extra" 2>/dev/null | awk "$DUMP_AWK" >> "$SYMFILE"
  fi
done

{
  echo "#!."
  sort -u "$SYMFILE"
} > "$OUTPUT"
rm -f "$SYMFILE"
