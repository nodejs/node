#!/bin/sh

# Notarize a generated node-<version>.pkg file as an Apple requirement for installation on macOS Catalina and later, as validated by Gatekeeper.
# Uses gon (Xcode version < 13.0) or notarytool (Xcode >= 13.0).

version() {
  echo "$@" | awk -F. '{ printf("%d%03d%03d%03d\n", $1,$2,$3,$4); }' || echo "0"
}

xcode_version=$(xcodebuild -version | awk '/Xcode/ {print $2}')
xcode_version_result=$(version "$xcode_version")
xcode_version_threshold=$(version "13.0")
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

# TODO(@ulisesGascon): remove support for gon
# when https://github.com/nodejs/build/issues/3385#issuecomment-1729281269 is ready
if [ "$xcode_version_result" -lt "$xcode_version_threshold" ]; then
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

  if ! command -v xcrun notarytool > /dev/null
  then
      echo "Notarytool is not present in the system. Notarization has failed."
      exit 1
  fi

  # Submit the package for notarization
  # TODO(@ulisesGascon): refactor to use --keychain-profile
  # when https://github.com/nodejs/build/issues/3385#issuecomment-1729281269 is ready
  notarization_output=$(
    xcrun notarytool submit \
      --apple-id "$NOTARIZATION_ID" \
      --password "$NOTARIZATION_PASSWORD" \
      --team-id "$NOTARIZATION_TEAM_ID" \
      --wait \
      "node-$pkgid.pkg" 2>&1
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
