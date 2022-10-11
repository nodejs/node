#!/bin/sh
set -e

# Shell script to update llhttp in the source tree to specific version

BASE_DIR="$( pwd )"/
DEPS_DIR="${BASE_DIR}deps/"
LLHTTP_VERSION="$1"

if [ "$#" -le 0 ]; then
  echo "Error: Please provide an llhttp version to update to."
  echo "Error: To download directly from GitHub, use the organization/repository syntax, without the .git suffix."
  exit 1
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

if echo "$LLHTTP_VERSION" | grep -qs "/" ; then # Download a release
  REPO="git@github.com:$LLHTTP_VERSION.git"
  BRANCH=$2
  [ -z "$BRANCH" ] && BRANCH=main

  echo "Cloning llhttp source archive $REPO ..."
  git clone "$REPO" llhttp
  cd llhttp
  echo "Checking out branch $BRANCH ..."
  git checkout "$BRANCH"

  echo "Building llhtttp ..."
  npm install
  make release

  echo "Copying llhtttp release ..."
  rm -rf "$DEPS_DIR/llhttp"
  cp -a release "$DEPS_DIR/llhttp"
else
  echo "Download llhttp release $LLHTTP_VERSION ..."
  curl -sL -o llhttp.tar.gz "https://github.com/nodejs/llhttp/archive/refs/tags/release/v$LLHTTP_VERSION.tar.gz"
  gzip -dc llhttp.tar.gz | tar xf -

  echo "Copying llhtttp release ..."
  rm -rf "$DEPS_DIR/llhttp"
  cp -a "llhttp-release-v$LLHTTP_VERSION" "$DEPS_DIR/llhttp"
fi

echo ""
echo "All done!"
echo ""
echo "Please git add llhttp, commit the new version:"
echo ""
echo "$ git add -A deps/llhttp"
echo "$ git commit -m \"deps: update llhttp to $LLHTTP_VERSION\""
echo ""
