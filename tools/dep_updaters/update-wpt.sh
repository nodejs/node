#!/bin/sh
set -e
# Shell script to update wpt test fixtures in the source tree to the most recent version.

BASE_DIR=$(cd "$(dirname "$0")/../.." && pwd)

cd "$BASE_DIR"
# If WPT_REVISION is empty, the latest revision will be used.
# shellcheck disable=SC2154
jq -r 'keys[]' "$BASE_DIR/test/fixtures/wpt/versions.json" | \
  xargs -L 1 git node wpt --commit "$WPT_REVISION"
