#!/bin/bash

set -euo pipefail

BUILTIN_NAME="$1"

if ! which rg >/dev/null ; then
    echo >&2 "This tool requires 'rg', install it with 'sudo apt install ripgrep'"
    exit 1
fi

TOOLS_DIRNAME="$(dirname "$0")"
V8_DIRNAME="$(dirname "$TOOLS_DIRNAME")"

if rg --type-add 'tq:*.tq' --type tq --with-filename --line-number "\bbuiltin $BUILTIN_NAME\b" "$V8_DIRNAME" | rg -v '\bextern builtin\b' | cut -f1-2 -d: ; then
    exit 0
fi

if rg --type cpp --with-filename --line-number "\b(TF_BUILTIN\(|::Generate_?)$BUILTIN_NAME\b" "$V8_DIRNAME" | cut -f1-2 -d: ; then
    exit 0
fi

echo >&2 "Builtin '$BUILTIN_NAME' not found"
exit 1