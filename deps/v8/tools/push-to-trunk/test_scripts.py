#!/usr/bin/env python
# Copyright 2013 the V8 project authors. All rights reserved.
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
#       copyright notice, this list of conditions and the following
#       disclaimer in the documentation and/or other materials provided
#       with the distribution.
#     * Neither the name of Google Inc. nor the names of its
#       contributors may be used to endorse or promote products derived
#       from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import os
import tempfile
import traceback
import unittest

import auto_push
from auto_push import CheckLastPush
from auto_push import SETTINGS_LOCATION
import common_includes
from common_includes import *
import merge_to_branch
from merge_to_branch import *
import push_to_trunk
from push_to_trunk import *
import chromium_roll
from chromium_roll import CHROMIUM
from chromium_roll import DEPS_FILE
from chromium_roll import ChromiumRoll


TEST_CONFIG = {
  BRANCHNAME: "test-prepare-push",
  TRUNKBRANCH: "test-trunk-push",
  PERSISTFILE_BASENAME: "/tmp/test-v8-push-to-trunk-tempfile",
  TEMP_BRANCH: "test-prepare-push-temporary-branch-created-by-script",
  DOT_GIT_LOCATION: None,
  VERSION_FILE: None,
  CHANGELOG_FILE: None,
  CHANGELOG_ENTRY_FILE: "/tmp/test-v8-push-to-trunk-tempfile-changelog-entry",
  PATCH_FILE: "/tmp/test-v8-push-to-trunk-tempfile-patch",
  COMMITMSG_FILE: "/tmp/test-v8-push-to-trunk-tempfile-commitmsg",
  CHROMIUM: "/tmp/test-v8-push-to-trunk-tempfile-chromium",
  DEPS_FILE: "/tmp/test-v8-push-to-trunk-tempfile-chromium/DEPS",
  SETTINGS_LOCATION: None,
  ALREADY_MERGING_SENTINEL_FILE:
      "/tmp/test-merge-to-branch-tempfile-already-merging",
  COMMIT_HASHES_FILE: "/tmp/test-merge-to-branch-tempfile-PATCH_COMMIT_HASHES",
  TEMPORARY_PATCH_FILE: "/tmp/test-merge-to-branch-tempfile-temporary-patch",
}


AUTO_PUSH_ARGS = [
  "-a", "author@chromium.org",
  "-r", "reviewer@chromium.org",
]


class ToplevelTest(unittest.TestCase):
  def testMakeComment(self):
    self.assertEquals("#   Line 1\n#   Line 2\n#",
                      MakeComment("    Line 1\n    Line 2\n"))
    self.assertEquals("#Line 1\n#Line 2",
                      MakeComment("Line 1\n Line 2"))

  def testStripComments(self):
    self.assertEquals("    Line 1\n    Line 3\n",
        StripComments("    Line 1\n#   Line 2\n    Line 3\n#\n"))
    self.assertEquals("\nLine 2 ### Test\n #",
        StripComments("###\n# \n\n#  Line 1\nLine 2 ### Test\n #"))

  def testMakeChangeLogBodySimple(self):
    commits = [
          ["Title text 1",
           "Title text 1\n\nBUG=\n",
           "author1@chromium.org"],
          ["Title text 2.",
           "Title text 2\n\nBUG=1234\n",
           "author2@chromium.org"],
        ]
    self.assertEquals("        Title text 1.\n"
                      "        (author1@chromium.org)\n\n"
                      "        Title text 2 (Chromium issue 1234).\n"
                      "        (author2@chromium.org)\n\n",
                      MakeChangeLogBody(commits))

  def testMakeChangeLogBodyEmpty(self):
    self.assertEquals("", MakeChangeLogBody([]))

  def testMakeChangeLogBodyAutoFormat(self):
    commits = [
          ["Title text 1!",
           "Title text 1\nLOG=y\nBUG=\n",
           "author1@chromium.org"],
          ["Title text 2",
           "Title text 2\n\nBUG=1234\n",
           "author2@chromium.org"],
          ["Title text 3",
           "Title text 3\n\nBUG=1234\nLOG = Yes\n",
           "author3@chromium.org"],
          ["Title text 3",
           "Title text 4\n\nBUG=1234\nLOG=\n",
           "author4@chromium.org"],
        ]
    self.assertEquals("        Title text 1.\n\n"
                      "        Title text 3 (Chromium issue 1234).\n\n",
                      MakeChangeLogBody(commits, True))

  def testRegressWrongLogEntryOnTrue(self):
    body = """
Check elimination: Learn from if(CompareMap(x)) on true branch.

BUG=
R=verwaest@chromium.org

Committed: https://code.google.com/p/v8/source/detail?r=18210
"""
    self.assertEquals("", MakeChangeLogBody([["title", body, "author"]], True))

  def testMakeChangeLogBugReferenceEmpty(self):
    self.assertEquals("", MakeChangeLogBugReference(""))
    self.assertEquals("", MakeChangeLogBugReference("LOG="))
    self.assertEquals("", MakeChangeLogBugReference(" BUG ="))
    self.assertEquals("", MakeChangeLogBugReference("BUG=none\t"))

  def testMakeChangeLogBugReferenceSimple(self):
    self.assertEquals("(issue 987654)",
                      MakeChangeLogBugReference("BUG = v8:987654"))
    self.assertEquals("(Chromium issue 987654)",
                      MakeChangeLogBugReference("BUG=987654 "))

  def testMakeChangeLogBugReferenceFromBody(self):
    self.assertEquals("(Chromium issue 1234567)",
                      MakeChangeLogBugReference("Title\n\nTBR=\nBUG=\n"
                                                " BUG=\tchromium:1234567\t\n"
                                                "R=somebody\n"))

  def testMakeChangeLogBugReferenceMultiple(self):
    # All issues should be sorted and grouped. Multiple references to the same
    # issue should be filtered.
    self.assertEquals("(issues 123, 234, Chromium issue 345)",
                      MakeChangeLogBugReference("Title\n\n"
                                                "BUG=v8:234\n"
                                                "  BUG\t= 345, \tv8:234,\n"
                                                "BUG=v8:123\n"
                                                "R=somebody\n"))
    self.assertEquals("(Chromium issues 123, 234)",
                      MakeChangeLogBugReference("Title\n\n"
                                                "BUG=234,,chromium:123 \n"
                                                "R=somebody\n"))
    self.assertEquals("(Chromium issues 123, 234)",
                      MakeChangeLogBugReference("Title\n\n"
                                                "BUG=chromium:234, , 123\n"
                                                "R=somebody\n"))
    self.assertEquals("(issues 345, 456)",
                      MakeChangeLogBugReference("Title\n\n"
                                                "\t\tBUG=v8:345,v8:456\n"
                                                "R=somebody\n"))
    self.assertEquals("(issue 123, Chromium issues 345, 456)",
                      MakeChangeLogBugReference("Title\n\n"
                                                "BUG=chromium:456\n"
                                                "BUG = none\n"
                                                "R=somebody\n"
                                                "BUG=456,v8:123, 345"))

  # TODO(machenbach): These test don't make much sense when the formatting is
  # done later.
  def testMakeChangeLogBugReferenceLong(self):
    # -----------------00--------10--------20--------30--------
    self.assertEquals("(issues 234, 1234567890, 1234567"
                      "8901234567890, Chromium issues 12345678,"
                      " 123456789)",
                      MakeChangeLogBugReference("BUG=v8:234\n"
                                                "BUG=v8:1234567890\n"
                                                "BUG=v8:12345678901234567890\n"
                                                "BUG=123456789\n"
                                                "BUG=12345678\n"))
    # -----------------00--------10--------20--------30--------
    self.assertEquals("(issues 234, 1234567890, 1234567"
                      "8901234567890, Chromium issues"
                      " 123456789, 1234567890)",
                      MakeChangeLogBugReference("BUG=v8:234\n"
                                                "BUG=v8:12345678901234567890\n"
                                                "BUG=v8:1234567890\n"
                                                "BUG=123456789\n"
                                                "BUG=1234567890\n"))
    # -----------------00--------10--------20--------30--------
    self.assertEquals("(Chromium issues 234, 1234567890"
                      ", 12345678901234567, "
                      "1234567890123456789)",
                      MakeChangeLogBugReference("BUG=234\n"
                                                "BUG=12345678901234567\n"
                                                "BUG=1234567890123456789\n"
                                                "BUG=1234567890\n"))


def Git(*args, **kwargs):
  """Convenience function returning a git test expectation."""
  return {
    "name": "git",
    "args": args[:-1],
    "ret": args[-1],
    "cb": kwargs.get("cb"),
  }


def RL(text, cb=None):
  """Convenience function returning a readline test expectation."""
  return {"name": "readline", "args": [], "ret": text, "cb": cb}


def URL(*args, **kwargs):
  """Convenience function returning a readurl test expectation."""
  return {
    "name": "readurl",
    "args": args[:-1],
    "ret": args[-1],
    "cb": kwargs.get("cb"),
  }


class SimpleMock(object):
  def __init__(self, name):
    self._name = name
    self._recipe = []
    self._index = -1

  def Expect(self, recipe):
    self._recipe = recipe

  def Call(self, name, *args):  # pragma: no cover
    self._index += 1
    try:
      expected_call = self._recipe[self._index]
    except IndexError:
      raise NoRetryException("Calling %s %s" % (name, " ".join(args)))

    if not isinstance(expected_call, dict):
      raise NoRetryException("Found wrong expectation type for %s %s"
                             % (name, " ".join(args)))


    # The number of arguments in the expectation must match the actual
    # arguments.
    if len(args) > len(expected_call['args']):
      raise NoRetryException("When calling %s with arguments, the "
          "expectations must consist of at least as many arguments.")

    # Compare expected and actual arguments.
    for (expected_arg, actual_arg) in zip(expected_call['args'], args):
      if expected_arg != actual_arg:
        raise NoRetryException("Expected: %s - Actual: %s"
                               % (expected_arg, actual_arg))

    # The expected call contains an optional callback for checking the context
    # at the time of the call.
    if expected_call['cb']:
      try:
        expected_call['cb']()
      except:
        tb = traceback.format_exc()
        raise NoRetryException("Caught exception from callback: %s" % tb)

    # If the return value is an exception, raise it instead of returning.
    if isinstance(expected_call['ret'], Exception):
      raise expected_call['ret']
    return expected_call['ret']

  def AssertFinished(self):  # pragma: no cover
    if self._index < len(self._recipe) -1:
      raise NoRetryException("Called %s too seldom: %d vs. %d"
                             % (self._name, self._index, len(self._recipe)))


class ScriptTest(unittest.TestCase):
  def MakeEmptyTempFile(self):
    handle, name = tempfile.mkstemp()
    os.close(handle)
    self._tmp_files.append(name)
    return name

  def WriteFakeVersionFile(self, build=4):
    with open(TEST_CONFIG[VERSION_FILE], "w") as f:
      f.write("  // Some line...\n")
      f.write("\n")
      f.write("#define MAJOR_VERSION    3\n")
      f.write("#define MINOR_VERSION    22\n")
      f.write("#define BUILD_NUMBER     %s\n" % build)
      f.write("#define PATCH_LEVEL      0\n")
      f.write("  // Some line...\n")
      f.write("#define IS_CANDIDATE_VERSION 0\n")

  def MakeStep(self):
    """Convenience wrapper."""
    options = ScriptsBase(TEST_CONFIG, self, self._state).MakeOptions([])
    return MakeStep(step_class=Step, state=self._state,
                    config=TEST_CONFIG, side_effect_handler=self,
                    options=options)

  def RunStep(self, script=PushToTrunk, step_class=Step, args=None):
    """Convenience wrapper."""
    args = args or ["-m"]
    return script(TEST_CONFIG, self, self._state).RunSteps([step_class], args)

  def GitMock(self, cmd, args="", pipe=True):
    print "%s %s" % (cmd, args)
    return self._git_mock.Call("git", args)

  def LogMock(self, cmd, args=""):
    print "Log: %s %s" % (cmd, args)

  MOCKS = {
    "git": GitMock,
    # TODO(machenbach): Little hack to reuse the git mock for the one svn call
    # in merge-to-branch. The command should be made explicit in the test
    # expectations.
    "svn": GitMock,
    "vi": LogMock,
  }

  def Call(self, fun, *args, **kwargs):
    print "Calling %s with %s and %s" % (str(fun), str(args), str(kwargs))

  def Command(self, cmd, args="", prefix="", pipe=True):
    return ScriptTest.MOCKS[cmd](self, cmd, args)

  def ReadLine(self):
    return self._rl_mock.Call("readline")

  def ReadURL(self, url, params):
    if params is not None:
      return self._url_mock.Call("readurl", url, params)
    else:
      return self._url_mock.Call("readurl", url)

  def Sleep(self, seconds):
    pass

  def GetDate(self):
    return "1999-07-31"

  def ExpectGit(self, *args):
    """Convenience wrapper."""
    self._git_mock.Expect(*args)

  def ExpectReadline(self, *args):
    """Convenience wrapper."""
    self._rl_mock.Expect(*args)

  def ExpectReadURL(self, *args):
    """Convenience wrapper."""
    self._url_mock.Expect(*args)

  def setUp(self):
    self._git_mock = SimpleMock("git")
    self._rl_mock = SimpleMock("readline")
    self._url_mock = SimpleMock("readurl")
    self._tmp_files = []
    self._state = {}

  def tearDown(self):
    Command("rm", "-rf %s*" % TEST_CONFIG[PERSISTFILE_BASENAME])

    # Clean up temps. Doesn't work automatically.
    for name in self._tmp_files:
      if os.path.exists(name):
        os.remove(name)

    self._git_mock.AssertFinished()
    self._rl_mock.AssertFinished()
    self._url_mock.AssertFinished()

  def testGitOrig(self):
    self.assertTrue(Command("git", "--version").startswith("git version"))

  def testGitMock(self):
    self.ExpectGit([Git("--version", "git version 1.2.3"), Git("dummy", "")])
    self.assertEquals("git version 1.2.3", self.MakeStep().Git("--version"))
    self.assertEquals("", self.MakeStep().Git("dummy"))

  def testCommonPrepareDefault(self):
    self.ExpectGit([
      Git("status -s -uno", ""),
      Git("status -s -b -uno", "## some_branch"),
      Git("svn fetch", ""),
      Git("branch", "  branch1\n* %s" % TEST_CONFIG[TEMP_BRANCH]),
      Git("branch -D %s" % TEST_CONFIG[TEMP_BRANCH], ""),
      Git("checkout -b %s" % TEST_CONFIG[TEMP_BRANCH], ""),
      Git("branch", ""),
    ])
    self.ExpectReadline([RL("Y")])
    self.MakeStep().CommonPrepare()
    self.MakeStep().PrepareBranch()
    self.assertEquals("some_branch", self._state["current_branch"])

  def testCommonPrepareNoConfirm(self):
    self.ExpectGit([
      Git("status -s -uno", ""),
      Git("status -s -b -uno", "## some_branch"),
      Git("svn fetch", ""),
      Git("branch", "  branch1\n* %s" % TEST_CONFIG[TEMP_BRANCH]),
    ])
    self.ExpectReadline([RL("n")])
    self.MakeStep().CommonPrepare()
    self.assertRaises(Exception, self.MakeStep().PrepareBranch)
    self.assertEquals("some_branch", self._state["current_branch"])

  def testCommonPrepareDeleteBranchFailure(self):
    self.ExpectGit([
      Git("status -s -uno", ""),
      Git("status -s -b -uno", "## some_branch"),
      Git("svn fetch", ""),
      Git("branch", "  branch1\n* %s" % TEST_CONFIG[TEMP_BRANCH]),
      Git("branch -D %s" % TEST_CONFIG[TEMP_BRANCH], None),
    ])
    self.ExpectReadline([RL("Y")])
    self.MakeStep().CommonPrepare()
    self.assertRaises(Exception, self.MakeStep().PrepareBranch)
    self.assertEquals("some_branch", self._state["current_branch"])

  def testInitialEnvironmentChecks(self):
    TEST_CONFIG[DOT_GIT_LOCATION] = self.MakeEmptyTempFile()
    os.environ["EDITOR"] = "vi"
    self.MakeStep().InitialEnvironmentChecks()

  def testReadAndPersistVersion(self):
    TEST_CONFIG[VERSION_FILE] = self.MakeEmptyTempFile()
    self.WriteFakeVersionFile(build=5)
    step = self.MakeStep()
    step.ReadAndPersistVersion()
    self.assertEquals("3", step["major"])
    self.assertEquals("22", step["minor"])
    self.assertEquals("5", step["build"])
    self.assertEquals("0", step["patch"])

  def testRegex(self):
    self.assertEqual("(issue 321)",
                     re.sub(r"BUG=v8:(.*)$", r"(issue \1)", "BUG=v8:321"))
    self.assertEqual("(Chromium issue 321)",
                     re.sub(r"BUG=(.*)$", r"(Chromium issue \1)", "BUG=321"))

    cl = "  too little\n\ttab\ttab\n         too much\n        trailing  "
    cl = MSub(r"\t", r"        ", cl)
    cl = MSub(r"^ {1,7}([^ ])", r"        \1", cl)
    cl = MSub(r"^ {9,80}([^ ])", r"        \1", cl)
    cl = MSub(r" +$", r"", cl)
    self.assertEqual("        too little\n"
                     "        tab        tab\n"
                     "        too much\n"
                     "        trailing", cl)

    self.assertEqual("//\n#define BUILD_NUMBER  3\n",
                     MSub(r"(?<=#define BUILD_NUMBER)(?P<space>\s+)\d*$",
                          r"\g<space>3",
                          "//\n#define BUILD_NUMBER  321\n"))

  def testPreparePushRevision(self):
    # Tests the default push hash used when the --revision option is not set.
    self.ExpectGit([
      Git("log -1 --format=%H HEAD", "push_hash")
    ])

    self.RunStep(PushToTrunk, PreparePushRevision)
    self.assertEquals("push_hash", self._state["push_hash"])

  def testPrepareChangeLog(self):
    TEST_CONFIG[VERSION_FILE] = self.MakeEmptyTempFile()
    self.WriteFakeVersionFile()
    TEST_CONFIG[CHANGELOG_ENTRY_FILE] = self.MakeEmptyTempFile()

    self.ExpectGit([
      Git("log --format=%H 1234..push_hash", "rev1\nrev2\nrev3\nrev4"),
      Git("log -1 --format=%s rev1", "Title text 1"),
      Git("log -1 --format=%B rev1", "Title\n\nBUG=\nLOG=y\n"),
      Git("log -1 --format=%an rev1", "author1@chromium.org"),
      Git("log -1 --format=%s rev2", "Title text 2."),
      Git("log -1 --format=%B rev2", "Title\n\nBUG=123\nLOG= \n"),
      Git("log -1 --format=%an rev2", "author2@chromium.org"),
      Git("log -1 --format=%s rev3", "Title text 3"),
      Git("log -1 --format=%B rev3", "Title\n\nBUG=321\nLOG=true\n"),
      Git("log -1 --format=%an rev3", "author3@chromium.org"),
      Git("log -1 --format=%s rev4", "Title text 4"),
      Git("log -1 --format=%B rev4",
       ("Title\n\nBUG=456\nLOG=Y\n\n"
        "Review URL: https://codereview.chromium.org/9876543210\n")),
      Git("log -1 --format=%an rev4", "author4@chromium.org"),
    ])

    # The cl for rev4 on rietveld has an updated LOG flag.
    self.ExpectReadURL([
      URL("https://codereview.chromium.org/9876543210/description",
          "Title\n\nBUG=456\nLOG=N\n\n"),
    ])

    self._state["last_push_bleeding_edge"] = "1234"
    self._state["push_hash"] = "push_hash"
    self._state["version"] = "3.22.5"
    self.RunStep(PushToTrunk, PrepareChangeLog)

    actual_cl = FileToText(TEST_CONFIG[CHANGELOG_ENTRY_FILE])

    expected_cl = """1999-07-31: Version 3.22.5

        Title text 1.

        Title text 3 (Chromium issue 321).

        Performance and stability improvements on all platforms.
#
# The change log above is auto-generated. Please review if all relevant
# commit messages from the list below are included.
# All lines starting with # will be stripped.
#
#       Title text 1.
#       (author1@chromium.org)
#
#       Title text 2 (Chromium issue 123).
#       (author2@chromium.org)
#
#       Title text 3 (Chromium issue 321).
#       (author3@chromium.org)
#
#       Title text 4 (Chromium issue 456).
#       (author4@chromium.org)
#
#"""

    self.assertEquals(expected_cl, actual_cl)

  def testEditChangeLog(self):
    TEST_CONFIG[CHANGELOG_ENTRY_FILE] = self.MakeEmptyTempFile()
    TextToFile("  New  \n\tLines  \n", TEST_CONFIG[CHANGELOG_ENTRY_FILE])
    os.environ["EDITOR"] = "vi"

    self.ExpectReadline([
      RL(""),  # Open editor.
    ])

    self.RunStep(PushToTrunk, EditChangeLog)

    self.assertEquals("New\n        Lines",
                      FileToText(TEST_CONFIG[CHANGELOG_ENTRY_FILE]))

  def testIncrementVersion(self):
    TEST_CONFIG[VERSION_FILE] = self.MakeEmptyTempFile()
    self.WriteFakeVersionFile()
    self._state["last_push_trunk"] = "hash1"

    self.ExpectGit([
      Git("checkout -f hash1 -- %s" % TEST_CONFIG[VERSION_FILE], "")
    ])

    self.ExpectReadline([
      RL("Y"),  # Increment build number.
    ])

    self.RunStep(PushToTrunk, IncrementVersion)

    self.assertEquals("3", self._state["new_major"])
    self.assertEquals("22", self._state["new_minor"])
    self.assertEquals("5", self._state["new_build"])
    self.assertEquals("0", self._state["new_patch"])

  def _TestSquashCommits(self, change_log, expected_msg):
    TEST_CONFIG[CHANGELOG_ENTRY_FILE] = self.MakeEmptyTempFile()
    with open(TEST_CONFIG[CHANGELOG_ENTRY_FILE], "w") as f:
      f.write(change_log)

    self.ExpectGit([
      Git("diff svn/trunk hash1", "patch content"),
      Git("svn find-rev hash1", "123455\n"),
    ])

    self._state["push_hash"] = "hash1"
    self._state["date"] = "1999-11-11"

    self.RunStep(PushToTrunk, SquashCommits)
    self.assertEquals(FileToText(TEST_CONFIG[COMMITMSG_FILE]), expected_msg)

    patch = FileToText(TEST_CONFIG[ PATCH_FILE])
    self.assertTrue(re.search(r"patch content", patch))

  def testSquashCommitsUnformatted(self):
    change_log = """1999-11-11: Version 3.22.5

        Log text 1.
        Chromium issue 12345

        Performance and stability improvements on all platforms.\n"""
    commit_msg = """Version 3.22.5 (based on bleeding_edge revision r123455)

Log text 1. Chromium issue 12345

Performance and stability improvements on all platforms."""
    self._TestSquashCommits(change_log, commit_msg)

  def testSquashCommitsFormatted(self):
    change_log = """1999-11-11: Version 3.22.5

        Long commit message that fills more than 80 characters (Chromium issue
        12345).

        Performance and stability improvements on all platforms.\n"""
    commit_msg = """Version 3.22.5 (based on bleeding_edge revision r123455)

Long commit message that fills more than 80 characters (Chromium issue 12345).

Performance and stability improvements on all platforms."""
    self._TestSquashCommits(change_log, commit_msg)

  def testSquashCommitsQuotationMarks(self):
    change_log = """Line with "quotation marks".\n"""
    commit_msg = """Line with "quotation marks"."""
    self._TestSquashCommits(change_log, commit_msg)

  def _PushToTrunk(self, force=False, manual=False):
    TEST_CONFIG[DOT_GIT_LOCATION] = self.MakeEmptyTempFile()

    # The version file on bleeding edge has build level 5, while the version
    # file from trunk has build level 4.
    TEST_CONFIG[VERSION_FILE] = self.MakeEmptyTempFile()
    self.WriteFakeVersionFile(build=5)

    TEST_CONFIG[CHANGELOG_ENTRY_FILE] = self.MakeEmptyTempFile()
    TEST_CONFIG[CHANGELOG_FILE] = self.MakeEmptyTempFile()
    bleeding_edge_change_log = "2014-03-17: Sentinel\n"
    TextToFile(bleeding_edge_change_log, TEST_CONFIG[CHANGELOG_FILE])
    os.environ["EDITOR"] = "vi"

    def ResetChangeLog():
      """On 'git co -b new_branch svn/trunk', and 'git checkout -- ChangeLog',
      the ChangLog will be reset to its content on trunk."""
      trunk_change_log = """1999-04-05: Version 3.22.4

        Performance and stability improvements on all platforms.\n"""
      TextToFile(trunk_change_log, TEST_CONFIG[CHANGELOG_FILE])

    def ResetToTrunk():
      ResetChangeLog()
      self.WriteFakeVersionFile()

    def CheckSVNCommit():
      commit = FileToText(TEST_CONFIG[COMMITMSG_FILE])
      self.assertEquals(
"""Version 3.22.5 (based on bleeding_edge revision r123455)

Log text 1 (issue 321).

Performance and stability improvements on all platforms.""", commit)
      version = FileToText(TEST_CONFIG[VERSION_FILE])
      self.assertTrue(re.search(r"#define MINOR_VERSION\s+22", version))
      self.assertTrue(re.search(r"#define BUILD_NUMBER\s+5", version))
      self.assertFalse(re.search(r"#define BUILD_NUMBER\s+6", version))
      self.assertTrue(re.search(r"#define PATCH_LEVEL\s+0", version))
      self.assertTrue(re.search(r"#define IS_CANDIDATE_VERSION\s+0", version))

      # Check that the change log on the trunk branch got correctly modified.
      change_log = FileToText(TEST_CONFIG[CHANGELOG_FILE])
      self.assertEquals(
"""1999-07-31: Version 3.22.5

        Log text 1 (issue 321).

        Performance and stability improvements on all platforms.


1999-04-05: Version 3.22.4

        Performance and stability improvements on all platforms.\n""",
          change_log)

    force_flag = " -f" if not manual else ""
    self.ExpectGit([
      Git("status -s -uno", ""),
      Git("status -s -b -uno", "## some_branch\n"),
      Git("svn fetch", ""),
      Git("branch", "  branch1\n* branch2\n"),
      Git("checkout -b %s" % TEST_CONFIG[TEMP_BRANCH], ""),
      Git("branch", "  branch1\n* branch2\n"),
      Git("branch", "  branch1\n* branch2\n"),
      Git("checkout -b %s svn/bleeding_edge" % TEST_CONFIG[BRANCHNAME], ""),
      Git("svn find-rev r123455", "push_hash\n"),
      Git(("log -1 --format=%H --grep="
           "\"^Version [[:digit:]]*\.[[:digit:]]*\.[[:digit:]]* (based\" "
           "svn/trunk"), "hash2\n"),
      Git("log -1 hash2", "Log message\n"),
      Git("log -1 --format=%s hash2",
       "Version 3.4.5 (based on bleeding_edge revision r1234)\n"),
      Git("svn find-rev r1234", "hash3\n"),
      Git("checkout -f hash2 -- %s" % TEST_CONFIG[VERSION_FILE], "",
          cb=self.WriteFakeVersionFile),
      Git("log --format=%H hash3..push_hash", "rev1\n"),
      Git("log -1 --format=%s rev1", "Log text 1.\n"),
      Git("log -1 --format=%B rev1", "Text\nLOG=YES\nBUG=v8:321\nText\n"),
      Git("log -1 --format=%an rev1", "author1@chromium.org\n"),
      Git("svn fetch", "fetch result\n"),
      Git("checkout -f svn/bleeding_edge", ""),
      Git("diff svn/trunk push_hash", "patch content\n"),
      Git("svn find-rev push_hash", "123455\n"),
      Git("checkout -b %s svn/trunk" % TEST_CONFIG[TRUNKBRANCH], "",
          cb=ResetToTrunk),
      Git("apply --index --reject \"%s\"" % TEST_CONFIG[PATCH_FILE], ""),
      Git("checkout -f svn/trunk -- %s" % TEST_CONFIG[CHANGELOG_FILE], "",
          cb=ResetChangeLog),
      Git("checkout -f svn/trunk -- %s" % TEST_CONFIG[VERSION_FILE], "",
          cb=self.WriteFakeVersionFile),
      Git("commit -aF \"%s\"" % TEST_CONFIG[COMMITMSG_FILE], "",
          cb=CheckSVNCommit),
      Git("svn dcommit 2>&1", "Some output\nCommitted r123456\nSome output\n"),
      Git("svn tag 3.22.5 -m \"Tagging version 3.22.5\"", ""),
      Git("checkout -f some_branch", ""),
      Git("branch -D %s" % TEST_CONFIG[TEMP_BRANCH], ""),
      Git("branch -D %s" % TEST_CONFIG[BRANCHNAME], ""),
      Git("branch -D %s" % TEST_CONFIG[TRUNKBRANCH], ""),
    ])

    # Expected keyboard input in manual mode:
    if manual:
      self.ExpectReadline([
        RL("Y"),  # Confirm last push.
        RL(""),  # Open editor.
        RL("Y"),  # Increment build number.
        RL("Y"),  # Sanity check.
      ])

    # Expected keyboard input in semi-automatic mode and forced mode:
    if not manual:
      self.ExpectReadline([])

    args = ["-a", "author@chromium.org", "--revision", "123455"]
    if force: args.append("-f")
    if manual: args.append("-m")
    else: args += ["-r", "reviewer@chromium.org"]
    PushToTrunk(TEST_CONFIG, self).Run(args)

    cl = FileToText(TEST_CONFIG[CHANGELOG_FILE])
    self.assertTrue(re.search(r"^\d\d\d\d\-\d+\-\d+: Version 3\.22\.5", cl))
    self.assertTrue(re.search(r"        Log text 1 \(issue 321\).", cl))
    self.assertTrue(re.search(r"1999\-04\-05: Version 3\.22\.4", cl))

    # Note: The version file is on build number 5 again in the end of this test
    # since the git command that merges to the bleeding edge branch is mocked
    # out.

  def testPushToTrunkManual(self):
    self._PushToTrunk(manual=True)

  def testPushToTrunkSemiAutomatic(self):
    self._PushToTrunk()

  def testPushToTrunkForced(self):
    self._PushToTrunk(force=True)


  def _ChromiumRoll(self, force=False, manual=False):
    TEST_CONFIG[DOT_GIT_LOCATION] = self.MakeEmptyTempFile()
    if not os.path.exists(TEST_CONFIG[CHROMIUM]):
      os.makedirs(TEST_CONFIG[CHROMIUM])
    TextToFile("Some line\n   \"v8_revision\": \"123444\",\n  some line",
               TEST_CONFIG[DEPS_FILE])

    os.environ["EDITOR"] = "vi"
    force_flag = " -f" if not manual else ""
    self.ExpectGit([
      Git("status -s -uno", ""),
      Git("status -s -b -uno", "## some_branch\n"),
      Git("svn fetch", ""),
      Git(("log -1 --format=%H --grep="
           "\"^Version [[:digit:]]*\.[[:digit:]]*\.[[:digit:]]* (based\" "
           "svn/trunk"), "push_hash\n"),
      Git("svn find-rev push_hash", "123455\n"),
      Git("log -1 --format=%s push_hash",
          "Version 3.22.5 (based on bleeding_edge revision r123454)\n"),
      Git("status -s -uno", ""),
      Git("checkout -f master", ""),
      Git("pull", ""),
      Git("checkout -b v8-roll-123455", ""),
      Git(("commit -am \"Update V8 to version 3.22.5 "
           "(based on bleeding_edge revision r123454).\n\n"
           "TBR=reviewer@chromium.org\""),
          ""),
      Git(("cl upload --send-mail --email \"author@chromium.org\"%s"
           % force_flag), ""),
    ])

    # Expected keyboard input in manual mode:
    if manual:
      self.ExpectReadline([
        RL("reviewer@chromium.org"),  # Chromium reviewer.
      ])

    # Expected keyboard input in semi-automatic mode and forced mode:
    if not manual:
      self.ExpectReadline([])

    args = ["-a", "author@chromium.org", "-c", TEST_CONFIG[CHROMIUM]]
    if force: args.append("-f")
    if manual: args.append("-m")
    else: args += ["-r", "reviewer@chromium.org"]
    ChromiumRoll(TEST_CONFIG, self).Run(args)

    deps = FileToText(TEST_CONFIG[DEPS_FILE])
    self.assertTrue(re.search("\"v8_revision\": \"123455\"", deps))

  def testChromiumRollManual(self):
    self._ChromiumRoll(manual=True)

  def testChromiumRollSemiAutomatic(self):
    self._ChromiumRoll()

  def testChromiumRollForced(self):
    self._ChromiumRoll(force=True)

  def testCheckLastPushRecently(self):
    self.ExpectGit([
      Git(("log -1 --format=%H --grep="
           "\"^Version [[:digit:]]*\.[[:digit:]]*\.[[:digit:]]* (based\" "
           "svn/trunk"), "hash2\n"),
      Git("log -1 --format=%s hash2",
          "Version 3.4.5 (based on bleeding_edge revision r99)\n"),
    ])

    self._state["lkgr"] = "101"

    self.assertRaises(Exception, lambda: self.RunStep(auto_push.AutoPush,
                                                      CheckLastPush,
                                                      AUTO_PUSH_ARGS))

  def testAutoPush(self):
    TEST_CONFIG[DOT_GIT_LOCATION] = self.MakeEmptyTempFile()
    TEST_CONFIG[SETTINGS_LOCATION] = "~/.doesnotexist"

    self.ExpectReadURL([
      URL("https://v8-status.appspot.com/current?format=json",
          "{\"message\": \"Tree is throttled\"}"),
      URL("https://v8-status.appspot.com/lkgr", Exception("Network problem")),
      URL("https://v8-status.appspot.com/lkgr", "100"),
    ])

    self.ExpectGit([
      Git("status -s -uno", ""),
      Git("status -s -b -uno", "## some_branch\n"),
      Git("svn fetch", ""),
      Git(("log -1 --format=%H --grep=\""
           "^Version [[:digit:]]*\.[[:digit:]]*\.[[:digit:]]* (based\""
           " svn/trunk"), "push_hash\n"),
      Git("log -1 --format=%s push_hash",
          "Version 3.4.5 (based on bleeding_edge revision r79)\n"),
    ])

    auto_push.AutoPush(TEST_CONFIG, self).Run(AUTO_PUSH_ARGS + ["--push"])

    state = json.loads(FileToText("%s-state.json"
                                  % TEST_CONFIG[PERSISTFILE_BASENAME]))

    self.assertEquals("100", state["lkgr"])

  def testAutoPushStoppedBySettings(self):
    TEST_CONFIG[DOT_GIT_LOCATION] = self.MakeEmptyTempFile()
    TEST_CONFIG[SETTINGS_LOCATION] = self.MakeEmptyTempFile()
    TextToFile("{\"enable_auto_push\": false}", TEST_CONFIG[SETTINGS_LOCATION])

    self.ExpectReadURL([])

    self.ExpectGit([
      Git("status -s -uno", ""),
      Git("status -s -b -uno", "## some_branch\n"),
      Git("svn fetch", ""),
    ])

    def RunAutoPush():
      auto_push.AutoPush(TEST_CONFIG, self).Run(AUTO_PUSH_ARGS)
    self.assertRaises(Exception, RunAutoPush)

  def testAutoPushStoppedByTreeStatus(self):
    TEST_CONFIG[DOT_GIT_LOCATION] = self.MakeEmptyTempFile()
    TEST_CONFIG[SETTINGS_LOCATION] = "~/.doesnotexist"

    self.ExpectReadURL([
      URL("https://v8-status.appspot.com/current?format=json",
          "{\"message\": \"Tree is throttled (no push)\"}"),
    ])

    self.ExpectGit([
      Git("status -s -uno", ""),
      Git("status -s -b -uno", "## some_branch\n"),
      Git("svn fetch", ""),
    ])

    def RunAutoPush():
      auto_push.AutoPush(TEST_CONFIG, self).Run(AUTO_PUSH_ARGS)
    self.assertRaises(Exception, RunAutoPush)

  def testMergeToBranch(self):
    TEST_CONFIG[ALREADY_MERGING_SENTINEL_FILE] = self.MakeEmptyTempFile()
    TEST_CONFIG[DOT_GIT_LOCATION] = self.MakeEmptyTempFile()
    TEST_CONFIG[VERSION_FILE] = self.MakeEmptyTempFile()
    self.WriteFakeVersionFile(build=5)
    os.environ["EDITOR"] = "vi"
    extra_patch = self.MakeEmptyTempFile()

    def VerifyPatch(patch):
      return lambda: self.assertEquals(patch,
          FileToText(TEST_CONFIG[TEMPORARY_PATCH_FILE]))

    msg = """Merged r12345, r23456, r34567, r45678, r56789 into trunk branch.

Title4

Title2

Title3

Title1

Title5

BUG=123,234,345,456,567,v8:123
LOG=N
"""

    def VerifySVNCommit():
      commit = FileToText(TEST_CONFIG[COMMITMSG_FILE])
      self.assertEquals(msg, commit)
      version = FileToText(TEST_CONFIG[VERSION_FILE])
      self.assertTrue(re.search(r"#define MINOR_VERSION\s+22", version))
      self.assertTrue(re.search(r"#define BUILD_NUMBER\s+5", version))
      self.assertTrue(re.search(r"#define PATCH_LEVEL\s+1", version))
      self.assertTrue(re.search(r"#define IS_CANDIDATE_VERSION\s+0", version))

    self.ExpectGit([
      Git("status -s -uno", ""),
      Git("status -s -b -uno", "## some_branch\n"),
      Git("svn fetch", ""),
      Git("branch", "  branch1\n* branch2\n"),
      Git("checkout -b %s" % TEST_CONFIG[TEMP_BRANCH], ""),
      Git("branch", "  branch1\n* branch2\n"),
      Git("checkout -b %s svn/trunk" % TEST_CONFIG[BRANCHNAME], ""),
      Git("log --format=%H --grep=\"Port r12345\" --reverse svn/bleeding_edge",
          "hash1\nhash2"),
      Git("svn find-rev hash1 svn/bleeding_edge", "45678"),
      Git("log -1 --format=%s hash1", "Title1"),
      Git("svn find-rev hash2 svn/bleeding_edge", "23456"),
      Git("log -1 --format=%s hash2", "Title2"),
      Git("log --format=%H --grep=\"Port r23456\" --reverse svn/bleeding_edge",
          ""),
      Git("log --format=%H --grep=\"Port r34567\" --reverse svn/bleeding_edge",
          "hash3"),
      Git("svn find-rev hash3 svn/bleeding_edge", "56789"),
      Git("log -1 --format=%s hash3", "Title3"),
      Git("svn find-rev r12345 svn/bleeding_edge", "hash4"),
      # Simulate svn being down which stops the script.
      Git("svn find-rev r23456 svn/bleeding_edge", None),
      # Restart script in the failing step.
      Git("svn find-rev r12345 svn/bleeding_edge", "hash4"),
      Git("svn find-rev r23456 svn/bleeding_edge", "hash2"),
      Git("svn find-rev r34567 svn/bleeding_edge", "hash3"),
      Git("svn find-rev r45678 svn/bleeding_edge", "hash1"),
      Git("svn find-rev r56789 svn/bleeding_edge", "hash5"),
      Git("log -1 --format=%s hash4", "Title4"),
      Git("log -1 --format=%s hash2", "Title2"),
      Git("log -1 --format=%s hash3", "Title3"),
      Git("log -1 --format=%s hash1", "Title1"),
      Git("log -1 --format=%s hash5", "Title5"),
      Git("log -1 hash4", "Title4\nBUG=123\nBUG=234"),
      Git("log -1 hash2", "Title2\n BUG = v8:123,345"),
      Git("log -1 hash3", "Title3\nLOG=n\nBUG=567, 456"),
      Git("log -1 hash1", "Title1"),
      Git("log -1 hash5", "Title5"),
      Git("log -1 -p hash4", "patch4"),
      Git("apply --index --reject \"%s\"" % TEST_CONFIG[TEMPORARY_PATCH_FILE],
          "", cb=VerifyPatch("patch4")),
      Git("log -1 -p hash2", "patch2"),
      Git("apply --index --reject \"%s\"" % TEST_CONFIG[TEMPORARY_PATCH_FILE],
          "", cb=VerifyPatch("patch2")),
      Git("log -1 -p hash3", "patch3"),
      Git("apply --index --reject \"%s\"" % TEST_CONFIG[TEMPORARY_PATCH_FILE],
          "", cb=VerifyPatch("patch3")),
      Git("log -1 -p hash1", "patch1"),
      Git("apply --index --reject \"%s\"" % TEST_CONFIG[TEMPORARY_PATCH_FILE],
          "", cb=VerifyPatch("patch1")),
      Git("log -1 -p hash5", "patch5\n"),
      Git("apply --index --reject \"%s\"" % TEST_CONFIG[TEMPORARY_PATCH_FILE],
          "", cb=VerifyPatch("patch5\n")),
      Git("apply --index --reject \"%s\"" % extra_patch, ""),
      Git("commit -aF \"%s\"" % TEST_CONFIG[COMMITMSG_FILE], ""),
      Git("cl upload --send-mail -r \"reviewer@chromium.org\"", ""),
      Git("checkout -f %s" % TEST_CONFIG[BRANCHNAME], ""),
      Git("cl presubmit", "Presubmit successfull\n"),
      Git("cl dcommit -f --bypass-hooks", "Closing issue\n", cb=VerifySVNCommit),
      Git("svn fetch", ""),
      Git("log -1 --format=%%H --grep=\"%s\" svn/trunk" % msg, "hash6"),
      Git("svn find-rev hash6", "1324"),
      Git(("copy -r 1324 https://v8.googlecode.com/svn/trunk "
           "https://v8.googlecode.com/svn/tags/3.22.5.1 -m "
           "\"Tagging version 3.22.5.1\""), ""),
      Git("checkout -f some_branch", ""),
      Git("branch -D %s" % TEST_CONFIG[TEMP_BRANCH], ""),
      Git("branch -D %s" % TEST_CONFIG[BRANCHNAME], ""),
    ])

    self.ExpectReadline([
      RL("Y"),  # Automatically add corresponding ports (34567, 56789)?
      RL("Y"),  # Automatically increment patch level?
      RL("reviewer@chromium.org"),  # V8 reviewer.
      RL("LGTM"),  # Enter LGTM for V8 CL.
    ])

    # r12345 and r34567 are patches. r23456 (included) and r45678 are the MIPS
    # ports of r12345. r56789 is the MIPS port of r34567.
    args = ["-f", "-p", extra_patch, "--branch", "trunk", "12345", "23456",
            "34567"]

    # The first run of the script stops because of the svn being down.
    self.assertRaises(GitFailedException,
        lambda: MergeToBranch(TEST_CONFIG, self).Run(args))

    # Test that state recovery after restarting the script works.
    args += ["-s", "3"]
    MergeToBranch(TEST_CONFIG, self).Run(args)


class SystemTest(unittest.TestCase):
  def testReload(self):
    step = MakeStep(step_class=PrepareChangeLog, number=0, state={}, config={},
                    side_effect_handler=DEFAULT_SIDE_EFFECT_HANDLER)
    body = step.Reload(
"""------------------------------------------------------------------------
r17997 | machenbach@chromium.org | 2013-11-22 11:04:04 +0100 (...) | 6 lines

Prepare push to trunk.  Now working on version 3.23.11.

R=danno@chromium.org

Review URL: https://codereview.chromium.org/83173002

------------------------------------------------------------------------""")
    self.assertEquals(
"""Prepare push to trunk.  Now working on version 3.23.11.

R=danno@chromium.org

Committed: https://code.google.com/p/v8/source/detail?r=17997""", body)
