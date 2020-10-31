#!/usr/bin/env python
# Copyright (C) 2019 The Android Open Source Project
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
"""Script to wrap protoc execution.

This script exists to work-around the bad depfile generation by protoc when
generating descriptors."""

from __future__ import print_function
import argparse
import os
import sys
import subprocess
import tempfile

from codecs import open


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--descriptor_set_out', default=None)
  parser.add_argument('--dependency_out', default=None)
  parser.add_argument('protoc')
  args, remaining = parser.parse_known_args()

  if args.dependency_out and args.descriptor_set_out:
    with tempfile.NamedTemporaryFile() as t:
      custom = [
          '--descriptor_set_out', args.descriptor_set_out, '--dependency_out',
          t.name
      ]
      subprocess.check_call([args.protoc] + custom + remaining)

      dependency_data = t.read().decode('utf-8')

    with open(args.dependency_out, 'w', encoding='utf-8') as f:
      f.write(args.descriptor_set_out + ":")
      f.write(dependency_data)
  else:
    subprocess.check_call(sys.argv[1:])


if __name__ == '__main__':
  sys.exit(main())
