#!/usr/bin/env python
# Copyright 2015 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import mergeinfo
import shutil
import unittest

from collections import namedtuple
from os import path
from subprocess import Popen, PIPE, check_call

TEST_CONFIG = {
  "GIT_REPO": "/tmp/test-v8-search-related-commits",
}

class TestMergeInfo(unittest.TestCase):

  base_dir = TEST_CONFIG["GIT_REPO"]

  def _execute_git(self, git_args):

    fullCommand = ["git", "-C", self.base_dir] + git_args
    p = Popen(args=fullCommand, stdin=PIPE,
        stdout=PIPE, stderr=PIPE)
    output, err = p.communicate()
    rc = p.returncode
    if rc != 0:
      raise Exception(err)
    return output

  def setUp(self):
    if path.exists(self.base_dir):
      shutil.rmtree(self.base_dir)

    check_call(["git", "init", self.base_dir])

    # Initial commit
    message = '''Initial commit'''

    self._make_empty_commit(message)

  def tearDown(self):
    if path.exists(self.base_dir):
      shutil.rmtree(self.base_dir)

  def _assert_correct_standard_result(
      self, result, all_commits, hash_of_first_commit):
    self.assertEqual(len(result), 1, "Master commit not found")
    self.assertTrue(
        result.get(hash_of_first_commit),
        "Master commit is wrong")

    self.assertEqual(
        len(result[hash_of_first_commit]),
        1,
        "Child commit not found")
    self.assertEqual(
        all_commits[2],
        result[hash_of_first_commit][0],
        "Child commit wrong")

  def _get_commits(self):
    commits = self._execute_git(
        ["log", "--format=%H", "--reverse"]).splitlines()
    return commits

  def _make_empty_commit(self, message):
    self._execute_git(["commit", "--allow-empty", "-m", message])
    return self._get_commits()[-1]

  def testCanDescribeCommit(self):
    commits = self._get_commits()
    hash_of_first_commit = commits[0]

    result = mergeinfo.describe_commit(
        self.base_dir,
        hash_of_first_commit).splitlines()

    self.assertEqual(
        result[0],
        'commit ' + hash_of_first_commit)

  def testCanDescribeCommitSingleLine(self):
    commits = self._get_commits()
    hash_of_first_commit = commits[0]

    result = mergeinfo.describe_commit(
        self.base_dir,
        hash_of_first_commit, True).splitlines()

    self.assertEqual(
        str(result[0]),
        str(hash_of_first_commit[0:7]) + ' Initial commit')

  def testSearchFollowUpCommits(self):
    commits = self._get_commits()
    hash_of_first_commit = commits[0]

    message = 'Follow-up commit of '  + hash_of_first_commit
    self._make_empty_commit(message)
    self._make_empty_commit(message)
    self._make_empty_commit(message)
    commits = self._get_commits()
    message = 'Not related commit'
    self._make_empty_commit(message)

    followups = mergeinfo.get_followup_commits(
        self.base_dir,
        hash_of_first_commit)
    self.assertEqual(set(followups), set(commits[1:]))

  def testSearchMerges(self):
    self._execute_git(['branch', 'test'])
    self._execute_git(['checkout', 'master'])
    message = 'real initial commit'
    self._make_empty_commit(message)
    commits = self._get_commits()
    hash_of_first_commit = commits[0]

    self._execute_git(['checkout', 'test'])
    message = 'Not related commit'
    self._make_empty_commit(message)

    # This should be found
    message = 'Merge '  + hash_of_first_commit
    hash_of_hit = self._make_empty_commit(message)

    # This should be ignored
    message = 'Cr-Branched-From: '  + hash_of_first_commit
    hash_of_ignored = self._make_empty_commit(message)

    self._execute_git(['checkout', 'master'])

    followups = mergeinfo.get_followup_commits(
        self.base_dir,
        hash_of_first_commit)

    # Check if follow ups and merges are not overlapping
    self.assertEqual(len(followups), 0)

    message = 'Follow-up commit of '  + hash_of_first_commit
    hash_of_followup = self._make_empty_commit(message)

    merges = mergeinfo.get_merge_commits(self.base_dir, hash_of_first_commit)
    # Check if follow up is ignored
    self.assertTrue(hash_of_followup not in merges)

    # Check for proper return of merges
    self.assertTrue(hash_of_hit in merges)
    self.assertTrue(hash_of_ignored not in merges)

  def testIsLkgr(self):
    commits = self._get_commits()
    hash_of_first_commit = commits[0]
    self._make_empty_commit('This one is the lkgr head')
    self._execute_git(['branch', 'remotes/origin/lkgr'])
    hash_of_not_lkgr = self._make_empty_commit('This one is not yet lkgr')

    self.assertTrue(mergeinfo.is_lkgr(
      self.base_dir, hash_of_first_commit))
    self.assertFalse(mergeinfo.is_lkgr(
      self.base_dir, hash_of_not_lkgr))

  def testShowFirstCanary(self):
    commits = self._get_commits()
    hash_of_first_commit = commits[0]

    self.assertEqual(mergeinfo.get_first_canary(
      self.base_dir, hash_of_first_commit), 'No Canary coverage')

    self._execute_git(['branch', 'remotes/origin/chromium/2345'])
    self._execute_git(['branch', 'remotes/origin/chromium/2346'])

    self.assertEqual(mergeinfo.get_first_canary(
      self.base_dir, hash_of_first_commit), '2345')

if __name__ == "__main__":
   unittest.main()
