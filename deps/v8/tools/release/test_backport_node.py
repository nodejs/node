#!/usr/bin/env python
# Copyright 2017 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import shutil
import subprocess
import sys
import tempfile
import unittest

from common_includes import FileToText
import backport_node

# Base paths.
BASE_DIR = os.path.dirname(os.path.abspath(__file__))
TEST_DATA = os.path.join(BASE_DIR, 'testdata')

def gitify(path):
  files = os.listdir(path)
  subprocess.check_call(['git', 'init'], cwd=path)
  subprocess.check_call(['git', 'add'] + files, cwd=path)
  subprocess.check_call(['git', 'commit', '-m', 'Initial'], cwd=path)

class TestUpdateNode(unittest.TestCase):
  def setUp(self):
    self.workdir = tempfile.mkdtemp(prefix='tmp_test_node_')

  def tearDown(self):
    shutil.rmtree(self.workdir)

  def testUpdate(self):
    v8_cwd = os.path.join(self.workdir, 'v8')
    node_cwd = os.path.join(self.workdir, 'node')

    # Set up V8 test fixture.
    shutil.copytree(src=os.path.join(TEST_DATA, 'v8'), dst=v8_cwd)
    gitify(v8_cwd)

    # Set up node test fixture.
    shutil.copytree(src=os.path.join(TEST_DATA, 'node'), dst=node_cwd)
    gitify(os.path.join(node_cwd))

    # Add a patch.
    with open(os.path.join(v8_cwd, 'v8_foo'), 'w') as f:
      f.write('zonk')
    subprocess.check_call(['git', 'add', 'v8_foo'], cwd=v8_cwd)
    subprocess.check_call(['git', 'commit', '-m', "Title\n\nBody"], cwd=v8_cwd)
    commit = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=v8_cwd).strip()

    # Run update script.
    backport_node.Main([v8_cwd, node_cwd, commit, "--no-review"])

    # Check message.
    message = subprocess.check_output(['git', 'log', '-1', '--format=%B'], cwd=node_cwd)
    self.assertIn('Original commit message:\n\n  Title\n\n  Body', message)

    # Check patch.
    gitlog = subprocess.check_output(
        ['git', 'diff', 'master', '--cached', '--', 'deps/v8/v8_foo'],
        cwd=node_cwd,
    )
    self.assertIn('+zonk', gitlog.strip())

    # Check version.
    version_file = os.path.join(node_cwd, "deps", "v8", "include", "v8-version.h")
    self.assertIn('#define V8_PATCH_LEVEL 4322', FileToText(version_file))

if __name__ == "__main__":
  unittest.main()
