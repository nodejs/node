#!/bin/bash
set -e

rustup component add miri
cargo miri setup

export MIRIFLAGS="-Zmiri-strict-provenance"

cargo miri test
cargo miri test --target mips64-unknown-linux-gnuabi64

# run with wrapping integer overflow instead of panic
cargo miri test --release
