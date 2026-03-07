#!/usr/bin/env bash
#
# Copyright 2024 The Fuchsia Authors
#
# Licensed under a BSD-style license <LICENSE-BSD>, Apache License, Version 2.0
# <LICENSE-APACHE or https://www.apache.org/licenses/LICENSE-2.0>, or the MIT
# license <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your option.
# This file may not be copied, modified, or distributed except according to
# those terms.

set -eo pipefail

if [[ "$1" == "--fix" ]]; then
    FMT_FLAGS=""
else
    FMT_FLAGS="--check"
fi

find . -iname '*.rs' -type f -not -path './target/*' -not -iname '*.expected.rs' -not -path './vendor/*' -not -path './tools/vendor/*' -print0 | xargs -0 --no-run-if-empty ./cargo.sh +nightly fmt $FMT_FLAGS -- >&2
