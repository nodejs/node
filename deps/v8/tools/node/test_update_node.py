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

import update_node

# Base paths.
BASE_DIR = os.path.dirname(os.path.abspath(__file__))
TEST_DATA = os.path.join(BASE_DIR, 'testdata')

# Expectations.
EXPECTED_GITIGNORE = """
/testing/gtest/*
!/testing/gtest/include
/testing/gtest/include/*
!/testing/gtest/include/gtest
/testing/gtest/include/gtest/*
!/testing/gtest/include/gtest/gtest_prod.h
!/third_party/jinja2
!/third_party/markupsafe
/unrelated
"""

EXPECTED_GIT_DIFF = """
 create mode 100644 deps/v8/base/trace_event/common/common
 rename deps/v8/baz/{delete_me => v8_new} (100%)
 delete mode 100644 deps/v8/include/v8-version.h
 rename deps/v8/{delete_me => new/v8_new} (100%)
 create mode 100644 deps/v8/third_party/jinja2/jinja2
 create mode 100644 deps/v8/third_party/markupsafe/markupsafe
 create mode 100644 deps/v8/v8_new
"""

ADDED_FILES = [
  'v8_new',
  'new/v8_new',
  'baz/v8_new',
  'testing/gtest/gtest_new',
  'testing/gtest/new/gtest_new',
  'testing/gtest/baz/gtest_new',
  'third_party/jinja2/jinja2',
  'third_party/markupsafe/markupsafe'
]

REMOVED_FILES = [
  'delete_me',
  'baz/delete_me',
  'testing/gtest/delete_me',
  'testing/gtest/baz/delete_me',
]

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
    for repository in update_node.SUB_REPOSITORIES:
      gitify(os.path.join(v8_cwd, *repository))

    # Set up node test fixture.
    shutil.copytree(src=os.path.join(TEST_DATA, 'node'), dst=node_cwd)
    gitify(os.path.join(node_cwd))

    # Add a patch.
    with open(os.path.join(v8_cwd, 'v8_foo'), 'w') as f:
      f.write('zonk')
    subprocess.check_call(['git', 'add', 'v8_foo'], cwd=v8_cwd)

    # Run update script.
    update_node.Main([v8_cwd, node_cwd, "--commit", "--with-patch"])

    # Check expectations.
    with open(os.path.join(node_cwd, 'deps', 'v8', '.gitignore')) as f:
      actual_gitignore = f.read()
    self.assertEquals(EXPECTED_GITIGNORE.strip(), actual_gitignore.strip())
    for f in ADDED_FILES:
      added_file = os.path.join(node_cwd, 'deps', 'v8', *f.split('/'))
      self.assertTrue(os.path.exists(added_file))
    for f in REMOVED_FILES:
      removed_file = os.path.join(node_cwd, 'deps', 'v8', *f.split('/'))
      self.assertFalse(os.path.exists(removed_file))
    gitlog = subprocess.check_output(
        ['git', 'diff', 'master', '--summary'],
        cwd=node_cwd,
    )
    self.assertEquals(EXPECTED_GIT_DIFF.strip(), gitlog.strip())

    # Check patch.
    gitlog = subprocess.check_output(
        ['git', 'diff', 'master', '--cached', '--', 'deps/v8/v8_foo'],
        cwd=node_cwd,
    )
    self.assertIn('+zonk', gitlog.strip())

if __name__ == "__main__":
  unittest.main()
