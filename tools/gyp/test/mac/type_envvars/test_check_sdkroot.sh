#!/bin/bash
# Copyright (c) 2014 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -e

# `xcodebuild -version` output looks like
#    Xcode 4.6.3
#    Build version 4H1503
# or like
#    Xcode 4.2
#    Build version 4C199
# or like
#    Xcode 3.2.6
#    Component versions: DevToolsCore-1809.0; DevToolsSupport-1806.0
#    BuildVersion: 10M2518
# Convert that to '0463', '0420' and '0326' respectively.
function xcodeversion() {
  xcodebuild -version | awk '/Xcode ([0-9]+\.[0-9]+(\.[0-9]+)?)/ {
    version = $2
    gsub(/\./, "", version)
    if (length(version) < 3) {
      version = version "0"
    }
    if (length(version) < 4) {
      version = "0" version
    }
  }
  END { print version }'
}

# Returns true if |string1| is smaller than |string2|.
# This function assumes that both strings represent Xcode version numbers
# as returned by |xcodeversion|.
function smaller() {
  local min="$(echo -ne "${1}\n${2}\n" | sort -n | head -n1)"
  test "${min}" != "${2}"
}

if [[ "$(xcodeversion)" < "0500" ]]; then
  # Xcode version is older than 5.0, check that SDKROOT is set but empty.
  [[ -z "${SDKROOT}" && -z "${SDKROOT-_}" ]]
else
  # Xcode version is newer than 5.0, check that SDKROOT is set.
  [[ "${SDKROOT}" == "$(xcodebuild -version -sdk '' Path)" ]]
fi
