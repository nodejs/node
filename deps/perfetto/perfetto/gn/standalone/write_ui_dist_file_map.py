#!/usr/bin/env python
# Copyright (C) 2020 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
""" Writes a TypeScript dict that contains SHA256s of the passed files.

The output looks like this:
{
  hex_digest: '6c761701c5840483833ffb0bd70ae155b2b3c70e8f667c7bd1f6abc98095930',
  files: {
    'frontend_bundle.js': 'sha256-2IVKK/3mEMlDdXNADyK03L1cANKbBpU+xue+vnLOcyo=',
    'index.html': 'sha256-ZRS1+Xh/dFZeWZi/dz8QMWg/8PYQHNdazsNX2oX8s70=',
    ...
  }
}
"""

from __future__ import print_function

import argparse
import base64
import hashlib
import os
import sys

from base64 import b64encode


def hash_file(file_path):
  hasher = hashlib.sha256()
  with open(file_path, 'rb') as f:
    for chunk in iter(lambda: f.read(32768), b''):
      hasher.update(chunk)
    return file_path, hasher.digest()


def hash_list_hex(args):
  hasher = hashlib.sha256()
  for arg in args:
    hasher.update(arg)
  return hasher.hexdigest()


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--out', help='Path of the output file')
  parser.add_argument(
      '--strip', help='Strips the leading path in the generated file list')
  parser.add_argument('file_list', nargs=argparse.REMAINDER)
  args = parser.parse_args()

  # Compute the hash of each file.
  digests = dict(map(hash_file, args.file_list))

  contents = '// __generated_by %s\n' % __file__
  contents += 'export const UI_DIST_MAP = {\n'
  contents += '  files: {\n'
  strip = args.strip + ('' if args.strip[-1] == os.path.sep else os.path.sep)
  for fname, digest in digests.items():
    if not fname.startswith(strip):
      raise Exception('%s must start with %s (--strip arg)' % (fname, strip))
    fname = fname[len(strip):]
    # We use b64 instead of hexdigest() because it's handy for handling fetch()
    # subresource integrity.
    contents += '    \'%s\': \'sha256-%s\',\n' % (fname, b64encode(digest))
  contents += '  },\n'

  # Compute the hash of the all resources' hashes.
  contents += '  hex_digest: \'%s\',\n' % hash_list_hex(digests.values())
  contents += '};\n'

  with open(args.out + '.tmp', 'w') as fout:
    fout.write(contents)
  os.rename(args.out + '.tmp', args.out)


if __name__ == '__main__':
  sys.exit(main())
