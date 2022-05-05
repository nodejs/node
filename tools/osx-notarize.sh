#!/bin/sh

# Uses gon, from https://github.com/mitchellh/gon, to notarize a generated node-<version>.pkg file
# with Apple for installation on macOS Catalina and later as validated by Gatekeeper.

set -e

gon_version="0.2.2"
gon_exe="${HOME}/.gon/gon_${gon_version}"

pkgid="$1"

[ -z "$pkgid" ] && \
  echo "Usage: $0 <pkgid>" \
  exit 1

# shellcheck disable=SC2154
[ -z "$NOTARIZATION_ID" ] && \
  echo "No NOTARIZATION_ID environment var. Skipping notarization." \
  exit 0

set -x

mkdir -p "${HOME}/.gon/"

if [ ! -f "${gon_exe}" ]; then
  curl -sL "https://github.com/mitchellh/gon/releases/download/v${gon_version}/gon_${gon_version}_macos.zip" -o "${gon_exe}.zip"
  (cd "${HOME}/.gon/" && rm -f gon && unzip "${gon_exe}.zip" && mv gon "${gon_exe}")
fi

sed -e "s/{{appleid}}/${NOTARIZATION_ID}/" -e "s/{{pkgid}}/${pkgid}/" tools/osx-gon-config.json.tmpl \
  > gon-config.json

"${gon_exe}" -log-level=info gon-config.json
