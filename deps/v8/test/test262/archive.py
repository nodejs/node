#!/usr/bin/env python
# Copyright 2016 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import tarfile
import time

# In GN we expect the path to a stamp file as an argument.
if len(sys.argv) == 2:
  STAMP_FILE = os.path.abspath(sys.argv[1])

os.chdir(os.path.dirname(os.path.abspath(__file__)))

# Workaround for slow grp and pwd calls.
tarfile.grp = None
tarfile.pwd = None

def filter_git(tar_info):
  if tar_info.name.startswith(os.path.join('data', '.git')) or \
      tar_info.name.startswith(os.path.join('harness', '.git')):
    return None
  else:
    tar_info.uname = tar_info.gname = "test262"
    return tar_info

with tarfile.open('data.tar', 'w') as tar:
  tar.add('data', filter=filter_git)
  tar.add('harness', filter=filter_git)
  tar.add('local-tests')

# Workaround for GN. We can't specify the tarfile as output because it's
# not in the product directory. Therefore we track running of this script
# with an extra stamp file in the product directory.
if len(sys.argv) == 2:
  with open(STAMP_FILE, 'w') as f:
    f.write(str(time.time()))
