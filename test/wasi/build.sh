#!/bin/bash

set -ex

for file in c/*.c; do
  filename=$(basename -- "$file")
  filename="${filename%.*}"

  /opt/wasi-sdk/bin/clang $file -target wasm32-unknown-wasi -o wasm/$filename.wasm
done
