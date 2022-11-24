#!/bin/sh
set -e
# Shell script to update OpenSSL in the source tree to a specific version
# Based on https://github.com/nodejs/node/blob/main/doc/contributing/maintaining-openssl.md

cleanup() {
  EXIT_CODE=$?
  [ -d "$WORKSPACE" ] && rm -rf "$WORKSPACE"
  exit $EXIT_CODE
}

download() {
  if [ -z "$1" ]; then
    echo "Error: please provide an OpenSSL version to update to"
    echo "	e.g. ./$0 download 3.0.7+quic1"
    exit 1
  fi

  OPENSSL_VERSION=$1
  echo "Making temporary workspace..."
  WORKSPACE=$(mktemp -d 2> /dev/null || mktemp -d -t 'tmp')


  cd "$WORKSPACE"

  echo "Fetching OpenSSL source archive..."
  curl -sL "https://api.github.com/repos/quictls/openssl/tarball/openssl-$OPENSSL_VERSION" | tar xzf -
  mv quictls-openssl-* openssl

  echo "Replacing existing OpenSSL..."
  rm -rf "$DEPS_DIR/openssl/openssl"
  mv "$WORKSPACE/openssl" "$DEPS_DIR/openssl/"

  echo "All done!"
  echo ""
  echo "Please git add openssl, and commit the new version:"
  echo ""
  echo "$ git add -A deps/openssl/openssl"
  echo "$ git commit -m \"deps: upgrade openssl sources to quictls/openssl-$OPENSSL_VERSION\""
  echo ""
}

regenerate() {
  command -v perl >/dev/null 2>&1 || { echo >&2 "Error: 'Perl' required but not installed."; exit 1; }
  command -v nasm >/dev/null 2>&1 || { echo >&2 "Error: 'nasm' required but not installed."; exit 1; }
  command -v as >/dev/null 2>&1 || { echo >&2 "Error: 'GNU as' required but not installed."; exit 1; }
  perl -e "use Text::Template">/dev/null 2>&1 || { echo >&2 "Error: 'Text::Template' Perl module required but not installed."; exit 1; }

  echo "Regenerating platform-dependent files..."

  make -C "$DEPS_DIR/openssl/config" clean
  # Needed for compatibility with nasm on 32-bit Windows
  # See https://github.com/nodejs/node/blob/main/doc/contributing/maintaining-openssl.md#2-execute-make-in-depsopensslconfig-directory
  sed -i 's/#ifdef/%ifdef/g' "$DEPS_DIR/openssl/openssl/crypto/perlasm/x86asm.pl"
  sed -i 's/#endif/%endif/g' "$DEPS_DIR/openssl/openssl/crypto/perlasm/x86asm.pl"
  make -C "$DEPS_DIR/openssl/config"

  echo "All done!"
  echo ""
  echo "Please commit the regenerated files:"
  echo ""
  echo "$ git add -A deps/openssl/config/archs deps/openssl/openssl"
  echo "$ git commit -m \"deps: update archs files for openssl\""
  echo ""
}

help() {
    echo "Shell script to update OpenSSL in the source tree to a specific version"
    echo "Sub-commands:"
    printf "%-23s %s\n" "help" "show help menu and commands"
    printf "%-23s %s\n" "download" "download and replace OpenSSL source code with new version"
    printf "%-23s %s\n" "regenerate" "regenerate platform-specific files"
    echo ""
    exit "${1:-0}"
}

main() {
  if [ ${#} -eq 0 ]; then
    help 0
  fi

  trap cleanup INT TERM EXIT

  BASE_DIR=$(cd "$(dirname "$0")/../.." && pwd)
  DEPS_DIR="$BASE_DIR/deps"

  case ${1} in
    help | download | regenerate )
      $1 "${2}"
    ;;
    * )
      echo "unknown command: $1"
      help 1
      exit 1
    ;;
  esac
}

main "$@"
