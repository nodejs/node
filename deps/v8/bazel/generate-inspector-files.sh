# Copyright 2021 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

BAZEL_OUT=$1

# Create a default GN output folder
gn gen out/inspector

# Generate inspector files
autoninja -C out/inspector src/inspector:inspector

# Create directories in bazel output folder
mkdir -p $BAZEL_OUT/include/inspector
mkdir -p $BAZEL_OUT/src/inspector/protocol

# Copy generated files to bazel output folder
cp out/inspector/gen/include/inspector/* $BAZEL_OUT/include/inspector/
cp out/inspector/gen/src/inspector/protocol/* $BAZEL_OUT/src/inspector/protocol/
