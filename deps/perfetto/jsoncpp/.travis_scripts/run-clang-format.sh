#!/usr/bin/env bash

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
python $DIR/run-clang-format.py -r $DIR/../src/**/ $DIR/../include/**/