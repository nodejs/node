# Copyright (C) 2017 The Android Open Source Project
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


def main():
  devnull = open(os.devnull, 'w')
  for clang in ('clang', 'clang-3.8', 'clang-3.5'):
    if subprocess.call(['which', clang], stdout=devnull, stderr=devnull) != 0:
      continue
    res = subprocess.check_output([clang, '-print-search-dirs']).decode("utf-8")
    for line in res.splitlines():
      if not line.startswith('libraries:'):
        continue
      libs = line.split('=', 1)[1].split(':')
      for lib in libs:
        if '/clang/' not in lib or not os.path.isdir(lib + '/lib'):
          continue
        print(os.path.abspath(lib))
        print(clang)
        print(clang.replace('clang', 'clang++'))
        return 0
  print('Could not find the LLVM lib dir')
  return 1


if __name__ == '__main__':
  sys.exit(main())
