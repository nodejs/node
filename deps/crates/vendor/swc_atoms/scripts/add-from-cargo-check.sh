#!/usr/bin/env bash
set -eu

export CARGO_TERM_COLOR=never

cargo check 2>&1 | grep 'no rules expected the token' | sed -e 's/error: no rules expected the token `"//' | sed -e 's/"`//'