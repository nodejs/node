#!/bin/sh
set -e

# Shell script to update llhttp in the source tree to specific version

BASE_DIR=$(cd "$(dirname "$0")/../.." && pwd)
DEPS_DIR="${BASE_DIR}/deps"

[ -z "$NODE" ] && NODE="$BASE_DIR/out/Release/node"
[ -x "$NODE" ] || NODE=$(command -v node)

# shellcheck disable=SC1091
. "$BASE_DIR/tools/dep_updaters/utils.sh"

NEW_VERSION="$("$NODE" --input-type=module <<'EOF'
const res = await fetch('https://api.github.com/repos/nodejs/llhttp/releases/latest');
if (!res.ok) throw new Error(`FetchError: ${res.status} ${res.statusText}`, { cause: res });
const { tag_name } = await res.json();
console.log(tag_name.replace('release/v', ''));
EOF
)"

CURRENT_MAJOR_VERSION=$(grep "#define LLHTTP_VERSION_MAJOR" ./deps/llhttp/include/llhttp.h | sed -n "s/^.*MAJOR \(.*\)/\1/p")
CURRENT_MINOR_VERSION=$(grep "#define LLHTTP_VERSION_MINOR" ./deps/llhttp/include/llhttp.h | sed -n "s/^.*MINOR \(.*\)/\1/p")
CURRENT_PATCH_VERSION=$(grep "#define LLHTTP_VERSION_PATCH" ./deps/llhttp/include/llhttp.h | sed -n "s/^.*PATCH \(.*\)/\1/p")
CURRENT_VERSION="$CURRENT_MAJOR_VERSION.$CURRENT_MINOR_VERSION.$CURRENT_PATCH_VERSION"

echo "Comparing $NEW_VERSION with $CURRENT_VERSION"

if [ "$NEW_VERSION" = "$CURRENT_VERSION" ]; then
  echo "Skipped because llhttp is on the latest version."
  exit 0
fi

cleanup () {
  EXIT_CODE=$?
  [ -d "$WORKSPACE" ] && rm -rf "$WORKSPACE"
  exit $EXIT_CODE
}

echo "Making temporary workspace ..."
WORKSPACE=$(mktemp -d 2> /dev/null || mktemp -d -t 'tmp')
trap cleanup INT TERM EXIT

cd "$WORKSPACE"

if echo "$NEW_VERSION" | grep -qs "/" ; then # Download a release
  REPO="git@github.com:$NEW_VERSION.git"
  BRANCH=$2
  [ -z "$BRANCH" ] && BRANCH=main

  echo "Cloning llhttp source archive $REPO ..."
  git clone "$REPO" llhttp
  cd llhttp
  echo "Checking out branch $BRANCH ..."
  git checkout "$BRANCH"

  echo "Building llhttp ..."
  npm install
  make release

  echo "Copying llhttp release ..."
  rm -rf "$DEPS_DIR/llhttp"
  cp -a release "$DEPS_DIR/llhttp"
else
  echo "Download llhttp release $NEW_VERSION ..."
  LLHTTP_TARBALL="llhttp-v$NEW_VERSION.tar.gz"
  curl -sL -o "$LLHTTP_TARBALL" "https://github.com/nodejs/llhttp/archive/refs/tags/release/v$NEW_VERSION.tar.gz"
  gzip -dc "$LLHTTP_TARBALL" | tar xf -

  echo "Copying llhttp release ..."
  rm -rf "$DEPS_DIR/llhttp"
  cp -a "llhttp-release-v$NEW_VERSION" "$DEPS_DIR/llhttp"
fi

echo ""
echo "All done!"
echo ""
echo "Please git add llhttp, commit the new version:"
echo ""
echo "$ git add -A deps/llhttp"
echo "$ git commit -m \"deps: update llhttp to $NEW_VERSION\""
echo ""

# The last line of the script should always print the new version,
# as we need to add it to $GITHUB_ENV variable.
echo "NEW_VERSION=$NEW_VERSION"
