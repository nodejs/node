#!/usr/bin/env python
# Copyright 2016 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import re
import subprocess

GCMOLE_PATH = os.path.dirname(os.path.abspath(__file__))
SHA1_PATH = os.path.join(GCMOLE_PATH, 'gcmole-tools.tar.gz.sha1')

if re.search(r'\bgcmole=1', os.environ.get('GYP_DEFINES', '')):
  subprocess.check_call([
    'download_from_google_storage',
    '-b', 'chrome-v8-gcmole',
    '-u', '--no_resume',
    '-s', SHA1_PATH,
    '--platform=linux*'
  ])
