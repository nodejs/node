#!/bin/bash
# Copyright (C) 2019 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# Should code live in this script or in the PERFETTO_TEST_SCRIPT script?
# You might argue: after all they are both part of the same repo? The difference
# is in temporal pinning.
# Code in this script is part of the Docker image that is manually pushed
# on the VMs. Everything in here is orthogonal to the evolution of the repo.
# Everything from /ramdisk instead will reflect the state of the repo at the
# point in time of the checkout.
# Things like script that upload performance data to dashboarads should probably
# be in the docker image. Things that depend on build artifacts should probably
# come from the repo.

set -eux

# CWD is /ci/ramdisk. In the CI this is based on a tmpfs ramdisk.

# Print env vars for debugging. They contain GN args and entrypoint.
date
env

mkdir src && cd src

if [[ $PERFETTO_TEST_GIT_REF == "file://"* ]]; then
# This is used only by tools/run_test_like_ci.
git clone -q --no-tags --single-branch --depth=1 "$PERFETTO_TEST_GIT_REF" .
else
git clone -q --no-tags --single-branch \
  https://android.googlesource.com/platform/external/perfetto.git .
git config user.email "ci-bot@perfetto.dev"
git config user.name "Perfetto CI"
git fetch -q origin "$PERFETTO_TEST_GIT_REF"

# We really want to test the result of the merge of the CL in ToT master. Don't
# really care about whether the CL passes the test at the time it was written.
git merge -q FETCH_HEAD -m merge
fi

# The android buildtools are huge due to the emulator, keep that as a separate
# cache and pack/unpack separately. It's worth  ~30s on each non-android test.
if [[ "$PERFETTO_TEST_GN_ARGS" =~ "android" ]]; then
PREBUILTS_ARCHIVE=/ci/cache/buildtools-$(date +%Y-%m-%d)-android.tar.lz4
else
PREBUILTS_ARCHIVE=/ci/cache/buildtools-$(date +%Y-%m-%d).tar.lz4
fi

# Clear stale buildtools caches after 24h.
(echo /ci/cache/buildtools-* | grep -v $PREBUILTS_ARCHIVE | xargs rm -f) || true

if [ -f $PREBUILTS_ARCHIVE ]; then
  lz4 -d $PREBUILTS_ARCHIVE | tar xf - || rm -f $PREBUILTS_ARCHIVE
  git reset --hard  # Just in case some versioned file gets overwritten.
fi


# By default ccache uses the mtime of the compiler. This doesn't work because
# our compilers are hermetic and their mtime is the time when we run
# install-build-deps. Given that the toolchain is rolled via install-build-deps
# we use that file as an identity function for the compiler check.
export CCACHE_COMPILERCHECK=string:$(shasum tools/install-build-deps)
export CCACHE_UMASK=000
export CCACHE_DEPEND=1
export CCACHE_MAXSIZE=8G
export CCACHE_DIR=/ci/cache/ccache
export CCACHE_SLOPPINESS=include_file_ctime,include_file_mtime
export CCACHE_COMPRESS=1
export CCACHE_COMPRESSLEVEL=4
mkdir -m 777 -p $CCACHE_DIR

export PERFETTO_TEST_GN_ARGS="${PERFETTO_TEST_GN_ARGS} cc_wrapper=\"ccache\""

export PERFETTO_TEST_NINJA_ARGS="-l 100"
$PERFETTO_TEST_SCRIPT

# The code after this point will NOT run if the test fails (because of set -e).

ccache --show-stats

# Populate the cache on the first run. Do that atomically so in case of races
# one random worker wins.
if [ ! -f $PREBUILTS_ARCHIVE ]; then
  TMPFILE=$(mktemp -p /ci/cache).tar.lz4
  # Add only git-ignored dirs to the cache.
  git check-ignore buildtools/* | xargs tar c | lz4 -z - $TMPFILE
  mv -f $TMPFILE $PREBUILTS_ARCHIVE
fi
