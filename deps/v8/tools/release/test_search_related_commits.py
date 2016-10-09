#!/usr/bin/env python
# Copyright 2015 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from collections import namedtuple
from os import path
import search_related_commits
import shutil
from subprocess import Popen, PIPE, check_call
import unittest


TEST_CONFIG = {
  "GIT_REPO": "/tmp/test-v8-search-related-commits",
}

class TestSearchRelatedCommits(unittest.TestCase):

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
    message = """[turbofan] Sanitize language mode for javascript operators.

    R=mstarzinger@chromium.org

    Review URL: https://codereview.chromium.org/1084243005

    Cr-Commit-Position: refs/heads/master@{#28059}"""
    self._make_empty_commit(message)

    message = """[crankshaft] Do some stuff

    R=hablich@chromium.org

    Review URL: https://codereview.chromium.org/1084243007

    Cr-Commit-Position: refs/heads/master@{#28030}"""

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

  def testSearchByCommitPosition(self):
    message = """Revert of some stuff.
    > Cr-Commit-Position: refs/heads/master@{#28059}
    R=mstarzinger@chromium.org

    Review URL: https://codereview.chromium.org/1084243005

    Cr-Commit-Position: refs/heads/master@{#28088}"""

    self._make_empty_commit(message)

    commits = self._get_commits()
    hash_of_first_commit = commits[0]

    result = search_related_commits.search_all_related_commits(
        self.base_dir, hash_of_first_commit, "HEAD", None)

    self._assert_correct_standard_result(result, commits, hash_of_first_commit)

  def testSearchByTitle(self):
    message = """Revert of some stuff.
    > [turbofan] Sanitize language mode for javascript operators.
    > Cr-Commit-Position: refs/heads/master@{#289}
    R=mstarzinger@chromium.org

    Review URL: https://codereview.chromium.org/1084243005

    Cr-Commit-Position: refs/heads/master@{#28088}"""

    self._make_empty_commit(message)

    commits = self._get_commits()
    hash_of_first_commit = commits[0]

    result = search_related_commits.search_all_related_commits(
        self.base_dir, hash_of_first_commit, "HEAD", None)

    self._assert_correct_standard_result(result, commits, hash_of_first_commit)

  def testSearchByHash(self):
    commits = self._get_commits()
    hash_of_first_commit = commits[0]

    message = """Revert of some stuff.
    > [turbofan] Sanitize language mode for javascript operators.
    > Reverting """ + hash_of_first_commit + """
    > R=mstarzinger@chromium.org

    Review URL: https://codereview.chromium.org/1084243005

    Cr-Commit-Position: refs/heads/master@{#28088}"""

    self._make_empty_commit(message)

    #Fetch again for an update
    commits = self._get_commits()
    hash_of_first_commit = commits[0]

    result = search_related_commits.search_all_related_commits(
        self.base_dir,
        hash_of_first_commit,
        "HEAD",
        None)

    self._assert_correct_standard_result(result, commits, hash_of_first_commit)

  def testConsiderSeparator(self):
    commits = self._get_commits()
    hash_of_first_commit = commits[0]

    # Related commits happen before separator so it is not a hit
    message = """Revert of some stuff: Not a hit
    > [turbofan] Sanitize language mode for javascript operators.
    > Reverting """ + hash_of_first_commit + """
    > R=mstarzinger@chromium.org

    Review URL: https://codereview.chromium.org/1084243005

    Cr-Commit-Position: refs/heads/master@{#28088}"""
    self._make_empty_commit(message)

    # Related commits happen before and after separator so it is a hit
    commit_pos_of_master = "27088"
    message = """Implement awesome feature: Master commit

    Review URL: https://codereview.chromium.org/1084243235

    Cr-Commit-Position: refs/heads/master@{#""" + commit_pos_of_master + "}"
    self._make_empty_commit(message)

    # Separator commit
    message = """Commit which is the origin of the branch

    Review URL: https://codereview.chromium.org/1084243456

    Cr-Commit-Position: refs/heads/master@{#28173}"""
    self._make_empty_commit(message)

    # Filler commit
    message = "Some unrelated commit: Not a hit"
    self._make_empty_commit(message)

    # Related commit after separator: a hit
    message = "Patch r" + commit_pos_of_master +""" done

    Review URL: https://codereview.chromium.org/1084243235

    Cr-Commit-Position: refs/heads/master@{#29567}"""
    self._make_empty_commit(message)

    #Fetch again for an update
    commits = self._get_commits()
    hash_of_first_commit = commits[0]
    hash_of_hit = commits[3]
    hash_of_separator = commits[4]
    hash_of_child_hit = commits[6]

    result = search_related_commits.search_all_related_commits(
        self.base_dir,
        hash_of_first_commit,
        "HEAD",
        hash_of_separator)

    self.assertTrue(result.get(hash_of_hit), "Hit not found")
    self.assertEqual(len(result), 1, "More than one hit found")
    self.assertEqual(
        len(result.get(hash_of_hit)),
        1,
        "More than one child hit found")
    self.assertEqual(
        result.get(hash_of_hit)[0],
        hash_of_child_hit,
        "Wrong commit found")

  def testPrettyPrint(self):
    message = """Revert of some stuff.
    > [turbofan] Sanitize language mode for javascript operators.
    > Cr-Commit-Position: refs/heads/master@{#289}
    R=mstarzinger@chromium.org

    Review URL: https://codereview.chromium.org/1084243005

    Cr-Commit-Position: refs/heads/master@{#28088}"""

    self._make_empty_commit(message)

    commits = self._get_commits()
    hash_of_first_commit = commits[0]
    OptionsStruct = namedtuple(
        "OptionsStruct",
        "git_dir of until all prettyprint separator verbose")
    options = OptionsStruct(
        git_dir= self.base_dir,
        of= [hash_of_first_commit],
        until= [commits[2]],
        all= True,
        prettyprint= True,
        separator = None,
        verbose=False)
    output = []
    for current_line in search_related_commits.main(options):
      output.append(current_line)

    self.assertIs(len(output), 2, "Not exactly two entries written")
    self.assertTrue(output[0].startswith("+"), "Master entry not marked with +")
    self.assertTrue(output[1].startswith("| "), "Child entry not marked with |")

  def testNothingFound(self):
    commits = self._get_commits()

    self._execute_git(["commit", "--allow-empty", "-m", "A"])
    self._execute_git(["commit", "--allow-empty", "-m", "B"])
    self._execute_git(["commit", "--allow-empty", "-m", "C"])
    self._execute_git(["commit", "--allow-empty", "-m", "D"])

    hash_of_first_commit = commits[0]
    result = search_related_commits.search_all_related_commits(
        self.base_dir,
        hash_of_first_commit,
        "HEAD",
        None)

    self.assertEqual(len(result), 0, "Results found where none should be.")


if __name__ == "__main__":
  #import sys;sys.argv = ['', 'Test.testName']
   unittest.main()
