#!/bin/sh
set -e
# Shell script to update inspector_protocol in the source tree to the version same with V8's.

BASE_DIR=$(cd "$(dirname "$0")/../.." && pwd)
DEPS_DIR="$BASE_DIR/deps"

# shellcheck disable=SC1091
. "$BASE_DIR/tools/dep_updaters/utils.sh"

echo "Making temporary workspace..."

WORKSPACE=$(mktemp -d 2> /dev/null || mktemp -d -t 'tmp')

cd "$WORKSPACE"

git clone https://chromium.googlesource.com/deps/inspector_protocol.git

INSPECTOR_PROTOCOL_DIR="$WORKSPACE/inspector_protocol"

echo "Comparing latest upstream with current revision"

set +e
python "$BASE_DIR/tools/inspector_protocol/roll.py" \
  --ip_src_upstream "$INSPECTOR_PROTOCOL_DIR" \
  --node_src_downstream "$BASE_DIR"
STATUS="$?"
set -e

if [ "$STATUS" = "0" ]; then
  echo "Skipped because inspector_protocol is on the latest version."
  exit 0
fi

python "$BASE_DIR/tools/inspector_protocol/roll.py" \
  --ip_src_upstream "$INSPECTOR_PROTOCOL_DIR" \
  --node_src_downstream "$BASE_DIR" \
  --force

NEW_VERSION=$(grep "Revision:" "$DEPS_DIR/inspector_protocol/README.node" | sed -n 's/^Revision: \([[:xdigit:]]\{36\}\).*$/\1/p')

# Update the version number on maintaining-dependencies.md
# and print the new version as the last line of the script as we need
# to add it to $GITHUB_ENV variable
finalize_version_update "inspector_protocol" "$NEW_VERSION"
