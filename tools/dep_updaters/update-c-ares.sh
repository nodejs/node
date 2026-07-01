#!/bin/sh
set -ex
# Shell script to update c-ares in the source tree to a specific version

BASE_DIR=$(cd "$(dirname "$0")/../.." && pwd)
DEPS_DIR="$BASE_DIR/deps"

[ -z "$NODE" ] && NODE="$BASE_DIR/out/Release/node"
[ -x "$NODE" ] || NODE=$(command -v node)

# shellcheck disable=SC1091
. "$BASE_DIR/tools/dep_updaters/utils.sh"

NEW_VERSION_METADATA="$("$NODE" --input-type=module <<'EOF'
const res = await fetch('https://api.github.com/repos/c-ares/c-ares/releases/latest',
  process.env.GITHUB_TOKEN && {
    headers: {
      "Authorization": `Bearer ${process.env.GITHUB_TOKEN}`
    },
  });
if (!res.ok) throw new Error(`FetchError: ${res.status} ${res.statusText}`, { cause: res });
const { tag_name, assets } = await res.json();
const { browser_download_url, name } = assets.find(({ name }) => name.endsWith('.tar.gz'));
if(!browser_download_url || !name) throw new Error('No tarball found');
console.log(`${tag_name.replace(/^v/, '')} ${browser_download_url}`);
EOF
)"

IFS=' ' read -r NEW_VERSION NEW_VERSION_URL <<EOF
$NEW_VERSION_METADATA
EOF

CURRENT_VERSION=$(grep "#define ARES_VERSION_STR" ./deps/cares/include/ares_version.h |  sed -n "s/^.*VERSION_STR \"\(.*\)\"/\1/p")

# This function exit with 0 if new version and current version are the same
compare_dependency_version "c-ares" "$NEW_VERSION" "$CURRENT_VERSION"

echo "Making temporary workspace"

WORKSPACE=$(mktemp -d 2> /dev/null || mktemp -d -t 'tmp')
ARES_TARBALL="$WORKSPACE/c-ares.tgz"

cleanup () {
  EXIT_CODE=$?
  [ -d "$WORKSPACE" ] && rm -rf "$WORKSPACE"
  exit $EXIT_CODE
}

trap cleanup INT TERM EXIT

echo "Fetching c-ares source archive"
curl -sL -o "$ARES_TARBALL" "$NEW_VERSION_URL"

echo "Verifying PGP signature"
curl -sL -o "$ARES_TARBALL.asc" "$NEW_VERSION_URL.asc"
gpgv --keyring "$BASE_DIR/tools/dep_updaters/c-ares.kbx" "${ARES_TARBALL}.asc" "${ARES_TARBALL}"

log_and_verify_sha256sum "c-ares" "$ARES_TARBALL"
tar -C "$WORKSPACE" -xzf "$ARES_TARBALL"

ARES_FOLDER=$(find "$WORKSPACE" -mindepth 1 -maxdepth 1 -type d -print -quit)

echo "Removing tests"
rm -rf "$ARES_FOLDER/test"

echo "Copying existing .gitignore, config, gyp and gn files"
cp -R "$DEPS_DIR/cares/config" "$ARES_FOLDER"
cp "$DEPS_DIR/cares/.gitignore" "$ARES_FOLDER"
cp "$DEPS_DIR/cares/cares.gyp" "$ARES_FOLDER"
cp "$DEPS_DIR/cares/"*.gn "$DEPS_DIR/cares/"*.gni "$ARES_FOLDER"

echo "Replacing existing c-ares"
replace_dir "$DEPS_DIR/cares" "$ARES_FOLDER"

echo "Updating cares.gyp"
"$NODE" "$ROOT/tools/dep_updaters/update-c-ares.mjs"

# Update the version number on maintaining-dependencies.md
# and print the new version as the last line of the script as we need
# to add it to $GITHUB_ENV variable
finalize_version_update "c-ares" "$NEW_VERSION"
