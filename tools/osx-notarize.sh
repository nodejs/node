#!/bin/bash
# Notarize a generated node-<version>.pkg file as an Apple requirement for installation on macOS Catalina and later, as validated by Gatekeeper.
# Uses gon (Xcode version < 13.0) or notarytool (Xcode >= 13.0).

set -e

xcode_version=$(xcodebuild -version | awk '/Xcode/ {print $2}')
pkgid="$1"

[ -z "$pkgid" ] && {
  echo "Usage: $0 <pkgid>"
  exit 1
}

# shellcheck disable=SC2154
[ -z "$NOTARIZATION_ID" ] && {
  echo "No NOTARIZATION_ID environment variable. Skipping notarization."
  exit 0
}

if [[ "$xcode_version" < "13.0" ]]; then
  echo "Notarization process is done with gon."
  set -x

  gon_version="0.2.2"
  gon_exe="${HOME}/.gon/gon_${gon_version}"

  mkdir -p "${HOME}/.gon/"

  if [ ! -f "${gon_exe}" ]; then
    curl -sL "https://github.com/mitchellh/gon/releases/download/v${gon_version}/gon_${gon_version}_macos.zip" -o "${gon_exe}.zip"
    (cd "${HOME}/.gon/" && rm -f gon && unzip "${gon_exe}.zip" && mv gon "${gon_exe}")
  fi

  sed -e "s/{{appleid}}/${NOTARIZATION_ID}/" -e "s/{{pkgid}}/${pkgid}/" tools/osx-gon-config.json.tmpl \
    > gon-config.json

  "${gon_exe}" -log-level=info gon-config.json

else
  echo "Notarization process is done with Notarytool."

  if ! command -v xcrun &> /dev/null || ! xcrun --find notarytool &> /dev/null; then
    echo "Notarytool is not present in the system. Notarization has failed."
    exit 1
  fi

  # Submit the package for notarization
  notarization_output=$(
    xcrun notarytool submit "node-$pkgid.pkg" \
      --apple-id "@env:NOTARIZATION_APPLE_ID" \
      --password "@env:NOTARIZATION_PASSWORD" \
      --team-id "@env:NOTARIZATION_TEAM_ID" \
      --wait 2>&1
  )

  if [ $? -eq 0 ]; then
    # Extract the operation ID from the output
    operation_id=$(echo "$notarization_output" | awk '/RequestUUID/ {print $NF}')
    echo "Notarization submitted. Operation ID: $operation_id"
    exit 0
  else
    echo "Notarization failed. Error: $notarization_output"
    exit 1
  fi
fi