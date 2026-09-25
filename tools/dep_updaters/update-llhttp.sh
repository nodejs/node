#!/bin/sh
set -ex

# Shell script to update llhttp in the source tree to specific version

BASE_DIR=$(cd "$(dirname "$0")/../.." && pwd)
DEPS_DIR="${BASE_DIR}/deps"

[ -z "$NODE" ] && NODE="$BASE_DIR/out/Release/node"
[ -x "$NODE" ] || NODE=$(command -v node)
NPM_DIR="$DEPS_DIR/npm/bin"
NPM="$NPM_DIR/npm-cli.js"

# shellcheck disable=SC1091
. "$BASE_DIR/tools/dep_updaters/utils.sh"

if [ -n "$LOCAL_COPY" ]; then
  NEW_VERSION=$(PACKAGE_JSON="$LOCAL_COPY/package.json" node -p "require(process.env.PACKAGE_JSON).version")
else
  NEW_VERSION="$("$NODE" --input-type=module <<'EOF'
  const res = await fetch('https://api.github.com/repos/nodejs/llhttp/releases/latest',
    process.env.GITHUB_TOKEN && {
      headers: {
        "Authorization": `Bearer ${process.env.GITHUB_TOKEN}`
      },
    });
  if (!res.ok) throw new Error(`FetchError: ${res.status} ${res.statusText}`, { cause: res });
  const { tag_name } = await res.json();
  console.log(tag_name.replace('release/v', ''));
EOF
)"
fi

CURRENT_MAJOR_VERSION=$(grep "#define LLHTTP_VERSION_MAJOR" ./deps/llhttp/include/llhttp.h | sed -n "s/^.*MAJOR \(.*\)/\1/p")
CURRENT_MINOR_VERSION=$(grep "#define LLHTTP_VERSION_MINOR" ./deps/llhttp/include/llhttp.h | sed -n "s/^.*MINOR \(.*\)/\1/p")
CURRENT_PATCH_VERSION=$(grep "#define LLHTTP_VERSION_PATCH" ./deps/llhttp/include/llhttp.h | sed -n "s/^.*PATCH \(.*\)/\1/p")
CURRENT_VERSION="$CURRENT_MAJOR_VERSION.$CURRENT_MINOR_VERSION.$CURRENT_PATCH_VERSION"

# This function exit with 0 if new version and current version are the same
compare_dependency_version "llhttp" "$NEW_VERSION" "$CURRENT_VERSION"

cleanup () {
  EXIT_CODE=$?
  [ -d "$WORKSPACE" ] && rm -rf "$WORKSPACE"
  exit $EXIT_CODE
}

echo "Making temporary workspace ..."
WORKSPACE=$(mktemp -d 2> /dev/null || mktemp -d -t 'tmp')
trap cleanup INT TERM EXIT

if [ -z "$LOCAL_COPY" ]; then
  echo "Downloading llhttp source..."
  curl -fsSL "https://github.com/nodejs/llhttp/archive/refs/tags/v$NEW_VERSION.tar.gz" | tar xz -C "$WORKSPACE"

  LOCAL_COPY="$(find "$WORKSPACE" -mindepth 1 -maxdepth 1 -type d -print -quit)"
fi

echo "Replacing existing llhttp (except GYP and GN build files)"
mv "$DEPS_DIR/llhttp/"*.gn "$DEPS_DIR/llhttp/"*.gni "$WORKSPACE/"

echo "Building llhttp ..."
(
  cd "$LOCAL_COPY"
  "$NODE" "$NPM" ci --ignore-scripts
  "$NODE" "$NPM" exec tsc
  RELEASE=$NEW_VERSION make release
)

echo "Overwriting vendored files..."
rm -rf "$DEPS_DIR/llhttp"
cp -a "$LOCAL_COPY/release" "$DEPS_DIR/llhttp"

mv "$WORKSPACE/"*.gn "$WORKSPACE/"*.gni "$DEPS_DIR/llhttp"

# Update the version number on maintaining-dependencies.md
# and print the new version as the last line of the script as we need
# to add it to $GITHUB_ENV variable
finalize_version_update "llhttp" "$NEW_VERSION"
