#!/bin/sh

# Notarize a generated node-<version>.pkg file as an Apple requirement for installation on macOS Catalina and later, as validated by Gatekeeper.
# Uses notarytool and requires Xcode >= 13.0.

pkgid="$1"

if [ -z "$pkgid" ]; then
  echo "Usage: $0 <pkgid>"
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

echo "Notarization process is done with Notarytool."

if ! command -v xcrun notarytool > /dev/null
then
    echo "Notarytool is not present in the system. Notarization has failed."
    exit 1
fi

echo "Submitting node-$pkgid.pkg for notarization..."

xcrun notarytool submit \
  --keychain-profile "NODE_RELEASE_PROFILE" \
  --wait \
  "node-$pkgid.pkg"

if [ $? -eq 0 ]; then
  echo "Notarization node-$pkgid.pkg submitted successfully."
else
  echo "Notarization node-$pkgid.pkg failed."
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

if ! retry xcrun spctl --assess --type install --context context:primary-signature --ignore-cache --verbose=2 "node-$pkgid.pkg"; then
  echo "error: Signature will not be accepted by Gatekeeper!" 1>&2
  exit 1
else
  echo "Verification was successful."
fi

if ! retry xcrun stapler staple "node-$pkgid.pkg"; then
  echo "error: Stapling node-$pkgid.pkg failed." 1>&2
  exit 1
fi
echo "Stapler was successful."