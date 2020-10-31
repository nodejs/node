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

import os
import subprocess
import sys


def main(argv):
  if len(argv) != 2:
    print('Usage: %s output_file.h' % argv[0])
    return 1
  script_dir = os.path.dirname(os.path.realpath(__file__))
  revision = subprocess.check_output(
      ['git', '-C', script_dir, 'rev-parse', 'HEAD']).strip()
  new_contents = '#define PERFETTO_GET_GIT_REVISION() "%s"\n' % revision
  out_file = argv[1]
  old_contents = ''
  if os.path.isfile(out_file):
    with open(out_file) as f:
      old_contents = f.read()
  if old_contents == new_contents:
    return 0
  with open(out_file, 'w') as f:
    f.write(new_contents)
  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv))
