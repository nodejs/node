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

case "$target" in
  *.pkg) kind=pkg ;;
  *) kind=binary ;;
esac

echo "Notarization process is done with Notarytool."

if ! command -v xcrun notarytool > /dev/null
then
    echo "Notarytool is not present in the system. Notarization has failed."
    exit 1
fi

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

if [ "$kind" = "pkg" ]; then
  submit "$target" || exit 1

  if ! retry xcrun spctl --assess --type install --context context:primary-signature --ignore-cache --verbose=2 "$target"; then
    echo "error: Signature will not be accepted by Gatekeeper!" 1>&2
    exit 1
  else
    echo "Verification was successful."
  fi

  if ! retry xcrun stapler staple "$target"; then
    echo "error: Stapling $target failed." 1>&2
    exit 1
  fi
  echo "Stapler was successful."
else
  # ditto preserves the code signature and resource forks, which plain zip
  # does not, so it is the archiver Apple documents for notarization uploads.
  # The zip only exists for the upload. Remove it on every exit path,
  # including a failed ditto or an interrupted build, the same way the
  # dep_updaters scripts clean their workspace.
  zip="$target.notarize.zip"
  cleanup () {
    EXIT_CODE=$?
    rm -f "$zip"
    exit $EXIT_CODE
  }
  trap cleanup INT TERM EXIT
  rm -f "$zip"
  if ! ditto -c -k --keepParent "$target" "$zip"; then
    echo "error: Could not create $zip for submission." 1>&2
    exit 1
  fi

  submit "$zip" || exit 1
  rm -f "$zip"

  # There is nothing to staple to a bare binary, so the only way to confirm
  # the ticket is live is to have codesign look it up. spctl cannot be used
  # here: it rejects command line tools with "does not seem to be an app".
  if ! retry codesign --verify --verbose=2 -R=notarized "$target"; then
    echo "error: $target will not be accepted by Gatekeeper!" 1>&2
    exit 1
  fi
  echo "Verification was successful."
fi
