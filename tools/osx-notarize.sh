#!/bin/sh

# Notarize a macOS release artifact as an Apple requirement for running or
# installing on macOS Catalina and later, as validated by Gatekeeper.
# Uses notarytool and requires Xcode >= 13.0.
#
# Usage: osx-notarize.sh <path>
#
#   <path> ending in .pkg: the installer is submitted as is, assessed with
#     spctl and the notarization ticket is stapled to it.
#   any other <path>: a Mach-O binary, such as bin/node from the release
#     tarball. Apple only accepts zip, dmg or pkg submissions, so the binary
#     is zipped for the upload and the zip is discarded afterwards. A bare
#     binary cannot carry a stapled ticket; Gatekeeper fetches it online by
#     the code directory hash, so the binary itself is left untouched and the
#     published checksums do not change.
#
# For backwards compatibility a bare version id (for example v26.13.0) is
# accepted and resolved to node-<version>.pkg.

# The notarization ticket can take a while to become available from Apple's
# servers after notarytool reports "Accepted", so retry with backoff.
retry() {
  delay=15
  attempt=1
  while ! "$@"; do
    if [ "$attempt" -ge 6 ]; then
      return 1
    fi
    echo "Attempt $attempt failed, retrying in ${delay}s..."
    sleep "$delay"
    attempt=$((attempt + 1))
    delay=$((delay * 2))
  done
}

submit() {
  echo "Submitting $1 for notarization..."
  if xcrun notarytool submit \
    --keychain-profile "NODE_RELEASE_PROFILE" \
    --wait \
    "$1"; then
    echo "Notarization $1 submitted successfully."
  else
    echo "Notarization $1 failed."
    return 1
  fi
}

# Remove the upload zip on every exit path, including a failed ditto or an
# interrupted build, the same way the dep_updaters scripts clean their
# workspace. $zip is only set by notarize_binary.
cleanup() {
  EXIT_CODE=$?
  if [ -n "$zip" ]; then
    rm -f "$zip"
  fi
  exit $EXIT_CODE
}

notarize_pkg() {
  submit "$1" || exit 1

  if ! retry xcrun spctl --assess --type install --context context:primary-signature --ignore-cache --verbose=2 "$1"; then
    echo "error: Signature will not be accepted by Gatekeeper!" 1>&2
    exit 1
  fi
  echo "Verification was successful."

  if ! retry xcrun stapler staple "$1"; then
    echo "error: Stapling $1 failed." 1>&2
    exit 1
  fi
  echo "Stapler was successful."
}

notarize_binary() {
  # ditto preserves the code signature and resource forks, which plain zip
  # does not, so it is the archiver Apple documents for notarization uploads.
  # The zip only exists for the upload and is removed by cleanup.
  zip="$1.notarize.zip"
  rm -f "$zip"
  if ! ditto -c -k --keepParent "$1" "$zip"; then
    echo "error: Could not create $zip for submission." 1>&2
    exit 1
  fi

  submit "$zip" || exit 1
  rm -f "$zip"

  # There is nothing to staple to a bare binary, so the only way to confirm
  # the ticket is live is to have codesign look it up. spctl cannot be used
  # here: it rejects command line tools with "does not seem to be an app".
  if ! retry codesign --verify --verbose=2 -R=notarized "$1"; then
    echo "error: $1 will not be accepted by Gatekeeper!" 1>&2
    exit 1
  fi
  echo "Verification was successful."
}

target="$1"

if [ -z "$target" ]; then
  echo "Usage: $0 <path-to-pkg-or-binary>"
  exit 1
fi

# shellcheck disable=SC2154
if [ -z "$NOTARIZATION_ID" ]; then
  echo "No NOTARIZATION_ID environment variable. Skipping notarization."
  exit 0
fi

if [ -z "$NOTARIZATION_PASSWORD" ]; then
  echo "No NOTARIZATION_PASSWORD environment variable. Skipping notarization."
  exit 0
fi

if [ -z "$NOTARIZATION_TEAM_ID" ]; then
  echo "No NOTARIZATION_TEAM_ID environment variable. Skipping notarization."
  exit 0
fi

# Legacy call form: a version id instead of a path.
if [ ! -e "$target" ] && [ -e "node-$target.pkg" ]; then
  target="node-$target.pkg"
fi

if [ ! -e "$target" ]; then
  echo "error: $target does not exist." 1>&2
  exit 1
fi

echo "Notarization process is done with Notarytool."

if ! command -v xcrun notarytool > /dev/null
then
    echo "Notarytool is not present in the system. Notarization has failed."
    exit 1
fi

trap cleanup INT TERM EXIT

case "$target" in
  *.pkg) notarize_pkg "$target" ;;
  *) notarize_binary "$target" ;;
esac
