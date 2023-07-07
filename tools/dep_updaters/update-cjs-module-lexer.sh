#!/bin/sh
set -e
# Shell script to update cjs-module-lexer in the source tree to a specific version

BASE_DIR=$(cd "$(dirname "$0")/../.." && pwd)

DEPS_DIR="$BASE_DIR/deps"
[ -z "$NODE" ] && NODE="$BASE_DIR/out/Release/node"
[ -x "$NODE" ] || NODE=$(command -v node)

NPM="$DEPS_DIR/npm/bin/npm-cli.js"

# shellcheck disable=SC1091
. "$BASE_DIR/tools/dep_updaters/utils.sh"

NEW_VERSION="$("$NODE" --input-type=module <<'EOF'
const res = await fetch('https://api.github.com/repos/nodejs/cjs-module-lexer/tags',
  process.env.GITHUB_TOKEN && {
    headers: {
      "Authorization": `Bearer ${process.env.GITHUB_TOKEN}`
    },
  });
if (!res.ok) throw new Error(`FetchError: ${res.status} ${res.statusText}`, { cause: res });
const tags = await res.json();
const { name } = tags.at(0)
console.log(name);
EOF
)"

CURRENT_VERSION=$("$NODE" -p "require('./deps/cjs-module-lexer/package.json').version")

# This function exit with 0 if new version and current version are the same
compare_dependency_version "cjs-module-lexer" "$NEW_VERSION" "$CURRENT_VERSION"

echo "Making temporary workspace"

WORKSPACE=$(mktemp -d 2> /dev/null || mktemp -d -t 'tmp')

cleanup () {
  EXIT_CODE=$?
  [ -d "$WORKSPACE" ] && rm -rf "$WORKSPACE"
  exit $EXIT_CODE
}

trap cleanup INT TERM EXIT

cd "$WORKSPACE"

"$NODE" "$NPM" init --yes

"$NODE" "$NPM" install --global-style --no-bin-links --ignore-scripts "cjs-module-lexer@$NEW_VERSION"

rm -rf "$DEPS_DIR/cjs-module-lexer"

mv node_modules/cjs-module-lexer "$DEPS_DIR/cjs-module-lexer"

# update cjs_module_lexer_version.h
cat > "$BASE_DIR/src/cjs_module_lexer_version.h" << EOL
// This is an auto generated file, please do not edit.
// Refer to tools/dep_updaters/update-cjs-module-lexer.sh
#ifndef SRC_CJS_MODULE_LEXER_VERSION_H_
#define SRC_CJS_MODULE_LEXER_VERSION_H_
#define CJS_MODULE_LEXER_VERSION "$NEW_VERSION"
#endif  // SRC_CJS_MODULE_LEXER_VERSION_H_
EOL

# Update the version number on maintaining-dependencies.md
# and print the new version as the last line of the script as we need
# to add it to $GITHUB_ENV variable
finalize_version_update "cjs-module-lexer" "$NEW_VERSION" "src/cjs_module_lexer_version.h"
