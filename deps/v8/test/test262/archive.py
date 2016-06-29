#!/usr/bin/env python
# Copyright 2016 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import tarfile

os.chdir(os.path.dirname(os.path.abspath(__file__)))

# Workaround for slow grp and pwd calls.
tarfile.grp = None
tarfile.pwd = None

def filter_git(tar_info):
  if tar_info.name.startswith(os.path.join('data', '.git')):
    return None
  else:
    tar_info.uname = tar_info.gname = "test262"
    return tar_info

with tarfile.open('data.tar', 'w') as tar:
  tar.add('data', filter=filter_git)
