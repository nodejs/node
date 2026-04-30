#!/bin/sh
set -e
# Shell script to update libffi in the source tree to a specific version

BASE_DIR=$(cd "$(dirname "$0")/../.." && pwd)
DEPS_DIR="$BASE_DIR/deps"

[ -z "$NODE" ] && NODE="$BASE_DIR/out/Release/node"
[ -x "$NODE" ] || NODE=$(command -v node)

# shellcheck disable=SC1091
. "$BASE_DIR/tools/dep_updaters/utils.sh"

NEW_VERSION_METADATA="$($NODE --input-type=module <<'EOF'
const res = await fetch('https://api.github.com/repos/libffi/libffi/releases/latest',
  process.env.GITHUB_TOKEN && {
    headers: {
      Authorization: `Bearer ${process.env.GITHUB_TOKEN}`,
    },
  });
if (!res.ok) throw new Error(`FetchError: ${res.status} ${res.statusText}`, { cause: res });
const { tag_name, assets } = await res.json();
const version = tag_name.replace(/^v/, '');
const tarballName = `libffi-${version}.tar.gz`;
const tarball = assets.find(({ name }) => name === tarballName);
if (!tarball?.browser_download_url) throw new Error(`No release tarball found for ${tag_name}`);
console.log(`${version} ${tarball.browser_download_url} ${tarballName}`);
EOF
)"

IFS=' ' read -r NEW_VERSION NEW_VERSION_URL LIBFFI_TARBALL <<EOF
$NEW_VERSION_METADATA
EOF

CURRENT_VERSION=$(grep "^LIBFFI_VERSION = " "$DEPS_DIR/libffi/generate-headers.py" | sed -n "s/^LIBFFI_VERSION = '\(.*\)'/\1/p")

# This function exit with 0 if new version and current version are the same
compare_dependency_version "libffi" "$NEW_VERSION" "$CURRENT_VERSION"

echo "Making temporary workspace..."

WORKSPACE=$(mktemp -d 2> /dev/null || mktemp -d -t 'tmp')

cleanup () {
  EXIT_CODE=$?
  [ -d "$WORKSPACE" ] && rm -rf "$WORKSPACE"
  exit $EXIT_CODE
}

trap cleanup INT TERM EXIT

cd "$WORKSPACE"

echo "Fetching libffi source archive..."
curl -sL -o "$LIBFFI_TARBALL" "$NEW_VERSION_URL"
log_and_verify_sha256sum "libffi" "$LIBFFI_TARBALL"
gzip -dc "$LIBFFI_TARBALL" | tar xf -
rm "$LIBFFI_TARBALL"

FOLDER=$(ls -d -- */)
mv "$FOLDER" libffi

echo "Copying existing Node.js-specific files"
cp "$DEPS_DIR/libffi/libffi.gyp" "$WORKSPACE/libffi"
cp "$DEPS_DIR/libffi/generate-headers.py" "$WORKSPACE/libffi"
cp "$DEPS_DIR/libffi/generate-configure-headers.py" "$WORKSPACE/libffi"
cp "$DEPS_DIR/libffi/generate-darwin-source-and-headers.py" "$WORKSPACE/libffi"

echo "Replacing existing libffi"
replace_dir "$DEPS_DIR/libffi" "$WORKSPACE/libffi"

NEW_VERSION_NUMBER=$($NODE -e "const [major, minor, patch] = process.argv[1].split('.'); console.log(String(Number(major)) + String(Number(minor)).padStart(2, '0') + String(Number(patch)).padStart(2, '0'));" "$NEW_VERSION")

echo "Updating generate-headers.py"
perl -0pi -e "s/^LIBFFI_VERSION = '.*'
/LIBFFI_VERSION = '$NEW_VERSION'
/m; s/^LIBFFI_VERSION_NUMBER = '.*'
/LIBFFI_VERSION_NUMBER = '$NEW_VERSION_NUMBER'
/m" "$DEPS_DIR/libffi/generate-headers.py"

# Update the version number on maintaining-dependencies.md
# and print the new version as the last line of the script as we need
# to add it to $GITHUB_ENV variable
finalize_version_update "libffi" "$NEW_VERSION"
