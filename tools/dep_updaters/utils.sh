#!/bin/sh

# This function logs the archive checksum and, if provided, compares it with
# the deposited checksum
#
# $1 is the package name e.g. 'acorn', 'ada', 'base64' etc. See the file
# https://github.com/nodejs/node/blob/main/doc/contributing/maintaining/maintaining-dependencies.md
# for a complete list of package name
# $2 is the downloaded archive
# $3 (optional) is the deposited sha256 cheksum. When provided, it is checked
# against the checksum generated from the archive
log_and_verify_sha256sum() {
  package_name="$1"
  archive="$2"
  checksum="$3"
  bsd_formatted_checksum=$(shasum -a 256 --tag "$archive")
  if [ -z "$3" ]; then
    echo "$bsd_formatted_checksum"
  else
    archive_checksum=$(shasum -a 256 "$archive")
    if [ "$checksum" = "$archive_checksum" ]; then
      echo "Valid $package_name checksum"
      echo "$bsd_formatted_checksum"
    else
      echo "ERROR - Invalid $package_name checksum:"
      echo "deposited: $checksum"
      echo "generated: $archive_checksum"
      exit 1
    fi
  fi
}
