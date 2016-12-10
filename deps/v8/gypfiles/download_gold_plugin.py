#!/usr/bin/env python
# Copyright 2015 the V8 project authors. All rights reserved.
# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Script to download LLVM gold plugin from google storage."""

import json
import os
import re
import platform
import shutil
import subprocess
import sys
import zipfile

# Bail out on windows and cygwin.
if "win" in platform.system().lower():
  # Python 2.7.6 hangs at the second path.insert command on windows. Works
  # with python 2.7.8.
  print "Gold plugin download not supported on windows."
  sys.exit(0)

SCRIPT_DIR = os.path.dirname(os.path.realpath(__file__))
CHROME_SRC = os.path.abspath(os.path.join(SCRIPT_DIR, os.pardir))
sys.path.insert(0, os.path.join(CHROME_SRC, 'tools'))

import find_depot_tools

DEPOT_PATH = find_depot_tools.add_depot_tools_to_path()
GSUTIL_PATH = os.path.join(DEPOT_PATH, 'gsutil.py')

LLVM_BUILD_PATH = os.path.join(CHROME_SRC, 'third_party', 'llvm-build',
                               'Release+Asserts')
CLANG_UPDATE_PY = os.path.join(CHROME_SRC, 'tools', 'clang', 'scripts',
                               'update.py')
CLANG_REVISION = os.popen(CLANG_UPDATE_PY + ' --print-revision').read().rstrip()

CLANG_BUCKET = 'gs://chromium-browser-clang/Linux_x64'

GOLD_PLUGIN_PATH = os.path.join(LLVM_BUILD_PATH, 'lib', 'LLVMgold.so')

sys.path.insert(0, os.path.join(CHROME_SRC, 'tools', 'clang', 'scripts'))

import update

def main():
  if not re.search(r'cfi_vptr=1', os.environ.get('GYP_DEFINES', '')):
    # Bailout if this is not a cfi build.
    print 'Skipping gold plugin download for non-cfi build.'
    return 0
  if (os.path.exists(GOLD_PLUGIN_PATH) and
      update.ReadStampFile().strip() == update.PACKAGE_VERSION):
    # Bailout if clang is up-to-date. This requires the script to be run before
    # the clang update step! I.e. afterwards clang would always be up-to-date.
    print 'Skipping gold plugin download. File present and clang up to date.'
    return 0

  # Make sure this works on empty checkouts (i.e. clang not downloaded yet).
  if not os.path.exists(LLVM_BUILD_PATH):
    os.makedirs(LLVM_BUILD_PATH)

  targz_name = 'llvmgold-%s.tgz' % CLANG_REVISION
  remote_path = '%s/%s' % (CLANG_BUCKET, targz_name)

  os.chdir(LLVM_BUILD_PATH)

  # TODO(pcc): Fix gsutil.py cp url file < /dev/null 2>&0
  # (currently aborts with exit code 1,
  # https://github.com/GoogleCloudPlatform/gsutil/issues/289) or change the
  # stdin->stderr redirect in update.py to do something else (crbug.com/494442).
  subprocess.check_call(['python', GSUTIL_PATH,
                         'cp', remote_path, targz_name],
                        stderr=open('/dev/null', 'w'))
  subprocess.check_call(['tar', 'xzf', targz_name])
  os.remove(targz_name)
  return 0

if __name__ == '__main__':
  sys.exit(main())
