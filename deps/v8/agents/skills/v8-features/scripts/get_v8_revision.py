#! /usr/bin/python3
# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import requests
import base64
import re
import sys
import argparse


def retrieve_v8_version(chromium_hash: str) -> str:
  v8_version_pattern = re.compile(' *\'v8_revision\': *\'([0-9a-f]{40})\',')
  request = requests.get(
      'https://chromium.googlesource.com/chromium/src.git/+/{0}/DEPS?format=TEXT'
      .format(chromium_hash))
  if request.status_code != 200:
    raise Exception(f'Unknown Chromium hash {chromium_hash}')
  content = base64.b64decode(request.text).decode('utf-8')
  for line in content.split('\n'):
    match = v8_version_pattern.fullmatch(line)
    if match is not None:
      return match.group(1)
  raise Exception(
      f'Could not find v8_revision in DEPS for Chromium hash {chromium_hash}')


if __name__ == '__main__':
  parser = argparse.ArgumentParser(
      description='Output the V8 Git hash corresponding to a Chromium Git hash')
  parser.add_argument('hash', help='Chromium commit hash')
  args = parser.parse_args()
  try:
    print(retrieve_v8_version(args.hash))
  except Exception as e:
    print(f'ERROR - {e}', file=sys.stderr)
    sys.exit(1)
