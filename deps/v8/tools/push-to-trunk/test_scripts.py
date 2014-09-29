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
import auto_roll
from auto_roll import CLUSTERFUZZ_API_KEY_FILE
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
import releases
from releases import Releases
import bump_up_version
from bump_up_version import BumpUpVersion
from bump_up_version import LastChangeBailout
from bump_up_version import LKGRVersionUpToDateBailout
from auto_tag import AutoTag


TEST_CONFIG = {
  BRANCHNAME: "test-prepare-push",
  TRUNKBRANCH: "test-trunk-push",
  PERSISTFILE_BASENAME: "/tmp/test-v8-push-to-trunk-tempfile",
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
  CLUSTERFUZZ_API_KEY_FILE: "/tmp/test-fake-cf-api-key",
}


AUTO_PUSH_ARGS = [
  "-a", "author@chromium.org",
  "-r", "reviewer@chromium.org",
]


class ToplevelTest(unittest.TestCase):
  def testSortBranches(self):
    S = releases.SortBranches
    self.assertEquals(["3.1", "2.25"], S(["2.25", "3.1"])[0:2])
    self.assertEquals(["3.0", "2.25"], S(["2.25", "3.0", "2.24"])[0:2])
    self.assertEquals(["3.11", "3.2"], S(["3.11", "3.2", "2.24"])[0:2])

  def testFilterDuplicatesAndReverse(self):
    F = releases.FilterDuplicatesAndReverse
    self.assertEquals([], F([]))
    self.assertEquals([["100", "10"]], F([["100", "10"]]))
    self.assertEquals([["99", "9"], ["100", "10"]],
                      F([["100", "10"], ["99", "9"]]))
    self.assertEquals([["98", "9"], ["100", "10"]],
                      F([["100", "10"], ["99", "9"], ["98", "9"]]))
    self.assertEquals([["98", "9"], ["99", "10"]],
                      F([["100", "10"], ["99", "10"], ["98", "9"]]))

  def testBuildRevisionRanges(self):
    B = releases.BuildRevisionRanges
    self.assertEquals({}, B([]))
    self.assertEquals({"10": "100"}, B([["100", "10"]]))
    self.assertEquals({"10": "100", "9": "99:99"},
                      B([["100", "10"], ["99", "9"]]))
    self.assertEquals({"10": "100", "9": "97:99"},
                      B([["100", "10"], ["98", "9"], ["97", "9"]]))
    self.assertEquals({"10": "100", "9": "99:99", "3": "91:98"},
                      B([["100", "10"], ["99", "9"], ["91", "3"]]))
    self.assertEquals({"13": "101", "12": "100:100", "9": "94:97",
                       "3": "91:93, 98:99"},
                      B([["101", "13"], ["100", "12"], ["98", "3"],
                         ["94", "9"], ["91", "3"]]))

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
          "expectations must consist of at least as many arguments." % name)

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

  def WriteFakeVersionFile(self, minor=22, build=4, patch=0):
    with open(TEST_CONFIG[VERSION_FILE], "w") as f:
      f.write("  // Some line...\n")
      f.write("\n")
      f.write("#define MAJOR_VERSION    3\n")
      f.write("#define MINOR_VERSION    %s\n" % minor)
      f.write("#define BUILD_NUMBER     %s\n" % build)
      f.write("#define PATCH_LEVEL      %s\n" % patch)
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
    args = args if args is not None else ["-m"]
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

  def ReadClusterFuzzAPI(self, api_key, **params):
    # TODO(machenbach): Use a mock for this and add a test that stops rolling
    # due to clustefuzz results.
    return []

  def Sleep(self, seconds):
    pass

  def GetDate(self):
    return "1999-07-31"

  def GetUTCStamp(self):
    return "100000"

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
      Git("branch", "  branch1\n* %s" % TEST_CONFIG[BRANCHNAME]),
      Git("branch -D %s" % TEST_CONFIG[BRANCHNAME], ""),
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
      Git("branch", "  branch1\n* %s" % TEST_CONFIG[BRANCHNAME]),
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
      Git("branch", "  branch1\n* %s" % TEST_CONFIG[BRANCHNAME]),
      Git("branch -D %s" % TEST_CONFIG[BRANCHNAME], None),
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

  # Version on trunk: 3.22.4.0. Version on master (bleeding_edge): 3.22.6.
  # Make sure that the increment is 3.22.7.0.
  def testIncrementVersion(self):
    TEST_CONFIG[VERSION_FILE] = self.MakeEmptyTempFile()
    self.WriteFakeVersionFile()
    self._state["last_push_trunk"] = "hash1"
    self._state["latest_build"] = "6"
    self._state["latest_version"] = "3.22.6.0"

    self.ExpectGit([
      Git("checkout -f hash1 -- %s" % TEST_CONFIG[VERSION_FILE], ""),
      Git("checkout -f svn/bleeding_edge -- %s" % TEST_CONFIG[VERSION_FILE],
          "", cb=lambda: self.WriteFakeVersionFile(22, 6)),
    ])

    self.ExpectReadline([
      RL("Y"),  # Increment build number.
    ])

    self.RunStep(PushToTrunk, IncrementVersion)

    self.assertEquals("3", self._state["new_major"])
    self.assertEquals("22", self._state["new_minor"])
    self.assertEquals("7", self._state["new_build"])
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
      Git("checkout -f svn/bleeding_edge -- %s" % TEST_CONFIG[VERSION_FILE],
          "", cb=self.WriteFakeVersionFile),
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
    googlers_mapping_py = "%s-mapping.py" % TEST_CONFIG[PERSISTFILE_BASENAME]
    with open(googlers_mapping_py, "w") as f:
      f.write("""
def list_to_dict(entries):
  return {"g_name@google.com": "c_name@chromium.org"}
def get_list():
  pass""")

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
           "\"^Version [[:digit:]]*\.[[:digit:]]*\.[[:digit:]]*\" "
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
           "Please reply to the V8 sheriff c_name@chromium.org in "
           "case of problems.\n\nTBR=c_name@chromium.org\""),
          ""),
      Git(("cl upload --send-mail --email \"author@chromium.org\"%s"
           % force_flag), ""),
    ])

    self.ExpectReadURL([
      URL("https://chromium-build.appspot.com/p/chromium/sheriff_v8.js",
          "document.write('g_name')"),
    ])

    # Expected keyboard input in manual mode:
    if manual:
      self.ExpectReadline([
        RL("c_name@chromium.org"),  # Chromium reviewer.
      ])

    # Expected keyboard input in semi-automatic mode and forced mode:
    if not manual:
      self.ExpectReadline([])

    args = ["-a", "author@chromium.org", "-c", TEST_CONFIG[CHROMIUM],
            "--sheriff", "--googlers-mapping", googlers_mapping_py]
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

  def testAutoRollExistingRoll(self):
    self.ExpectReadURL([
      URL("https://codereview.chromium.org/search",
          "owner=author%40chromium.org&limit=30&closed=3&format=json",
          ("{\"results\": [{\"subject\": \"different\"},"
           "{\"subject\": \"Update V8 to Version...\"}]}")),
    ])

    result = auto_roll.AutoRoll(TEST_CONFIG, self).Run(
        AUTO_PUSH_ARGS + ["-c", TEST_CONFIG[CHROMIUM]])
    self.assertEquals(1, result)

  # Snippet from the original DEPS file.
  FAKE_DEPS = """
vars = {
  "v8_revision": "123455",
}
deps = {
  "src/v8":
    (Var("googlecode_url") % "v8") + "/" + Var("v8_branch") + "@" +
    Var("v8_revision"),
}
"""

  def testAutoRollUpToDate(self):
    self.ExpectReadURL([
      URL("https://codereview.chromium.org/search",
          "owner=author%40chromium.org&limit=30&closed=3&format=json",
          ("{\"results\": [{\"subject\": \"different\"}]}")),
      URL("http://src.chromium.org/svn/trunk/src/DEPS",
          self.FAKE_DEPS),
    ])

    self.ExpectGit([
      Git(("log -1 --format=%H --grep="
           "\"^Version [[:digit:]]*\.[[:digit:]]*\.[[:digit:]]*\" "
           "svn/trunk"), "push_hash\n"),
      Git("svn find-rev push_hash", "123455\n"),
    ])

    result = auto_roll.AutoRoll(TEST_CONFIG, self).Run(
        AUTO_PUSH_ARGS + ["-c", TEST_CONFIG[CHROMIUM]])
    self.assertEquals(1, result)

  def testAutoRoll(self):
    TEST_CONFIG[CLUSTERFUZZ_API_KEY_FILE]  = self.MakeEmptyTempFile()
    TextToFile("fake key", TEST_CONFIG[CLUSTERFUZZ_API_KEY_FILE])
    self.ExpectReadURL([
      URL("https://codereview.chromium.org/search",
          "owner=author%40chromium.org&limit=30&closed=3&format=json",
          ("{\"results\": [{\"subject\": \"different\"}]}")),
      URL("http://src.chromium.org/svn/trunk/src/DEPS",
          self.FAKE_DEPS),
    ])

    self.ExpectGit([
      Git(("log -1 --format=%H --grep="
           "\"^Version [[:digit:]]*\.[[:digit:]]*\.[[:digit:]]*\" "
           "svn/trunk"), "push_hash\n"),
      Git("svn find-rev push_hash", "123456\n"),
    ])

    result = auto_roll.AutoRoll(TEST_CONFIG, self).Run(
        AUTO_PUSH_ARGS + ["-c", TEST_CONFIG[CHROMIUM], "--roll"])
    self.assertEquals(0, result)

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

    msg = """Version 3.22.5.1 (merged r12345, r23456, r34567, r45678, r56789)

Title4

Title2

Title3

Title1

Revert "Something"

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
      Git("log -1 --format=%s hash5", "Revert \"Something\""),
      Git("log -1 hash4", "Title4\nBUG=123\nBUG=234"),
      Git("log -1 hash2", "Title2\n BUG = v8:123,345"),
      Git("log -1 hash3", "Title3\nLOG=n\nBUG=567, 456"),
      Git("log -1 hash1", "Title1\nBUG="),
      Git("log -1 hash5", "Revert \"Something\"\nBUG=none"),
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
      Git(("log -1 --format=%%H --grep=\"%s\" svn/trunk"
           % msg.replace("\"", "\\\"")), "hash6"),
      Git("svn find-rev hash6", "1324"),
      Git(("copy -r 1324 https://v8.googlecode.com/svn/trunk "
           "https://v8.googlecode.com/svn/tags/3.22.5.1 -m "
           "\"Tagging version 3.22.5.1\""), ""),
      Git("checkout -f some_branch", ""),
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

  def testReleases(self):
    tag_response_text = """
------------------------------------------------------------------------
r22631 | author1@chromium.org | 2014-07-28 02:05:29 +0200 (Mon, 28 Jul 2014)
Changed paths:
   A /tags/3.28.43 (from /trunk:22630)

Tagging version 3.28.43
------------------------------------------------------------------------
r22629 | author2@chromium.org | 2014-07-26 05:09:29 +0200 (Sat, 26 Jul 2014)
Changed paths:
   A /tags/3.28.41 (from /branches/bleeding_edge:22626)

Tagging version 3.28.41
------------------------------------------------------------------------
r22556 | author3@chromium.org | 2014-07-23 13:31:59 +0200 (Wed, 23 Jul 2014)
Changed paths:
   A /tags/3.27.34.7 (from /branches/3.27:22555)

Tagging version 3.27.34.7
------------------------------------------------------------------------
r22627 | author4@chromium.org | 2014-07-26 01:39:15 +0200 (Sat, 26 Jul 2014)
Changed paths:
   A /tags/3.28.40 (from /branches/bleeding_edge:22624)

Tagging version 3.28.40
------------------------------------------------------------------------
"""
    json_output = self.MakeEmptyTempFile()
    csv_output = self.MakeEmptyTempFile()
    TEST_CONFIG[VERSION_FILE] = self.MakeEmptyTempFile()
    self.WriteFakeVersionFile()

    TEST_CONFIG[DOT_GIT_LOCATION] = self.MakeEmptyTempFile()
    if not os.path.exists(TEST_CONFIG[CHROMIUM]):
      os.makedirs(TEST_CONFIG[CHROMIUM])
    def WriteDEPS(revision):
      TextToFile("Line\n   \"v8_revision\": \"%s\",\n  line\n" % revision,
                 TEST_CONFIG[DEPS_FILE])
    WriteDEPS(567)

    def ResetVersion(minor, build, patch=0):
      return lambda: self.WriteFakeVersionFile(minor=minor,
                                               build=build,
                                               patch=patch)

    def ResetDEPS(revision):
      return lambda: WriteDEPS(revision)

    self.ExpectGit([
      Git("status -s -uno", ""),
      Git("status -s -b -uno", "## some_branch\n"),
      Git("svn fetch", ""),
      Git("branch", "  branch1\n* branch2\n"),
      Git("checkout -b %s" % TEST_CONFIG[BRANCHNAME], ""),
      Git("branch -r", "  svn/3.21\n  svn/3.3\n"),
      Git("reset --hard svn/3.3", ""),
      Git("log --format=%H", "hash1\nhash2"),
      Git("diff --name-only hash1 hash1^", ""),
      Git("diff --name-only hash2 hash2^", TEST_CONFIG[VERSION_FILE]),
      Git("checkout -f hash2 -- %s" % TEST_CONFIG[VERSION_FILE], "",
          cb=ResetVersion(3, 1, 1)),
      Git("log -1 --format=%B hash2",
          "Version 3.3.1.1 (merged 12)\n\nReview URL: fake.com\n"),
      Git("log -1 --format=%s hash2", ""),
      Git("svn find-rev hash2", "234"),
      Git("log -1 --format=%ci hash2", "18:15"),
      Git("checkout -f HEAD -- %s" % TEST_CONFIG[VERSION_FILE], "",
          cb=ResetVersion(22, 5)),
      Git("reset --hard svn/3.21", ""),
      Git("log --format=%H", "hash3\nhash4\nhash5\n"),
      Git("diff --name-only hash3 hash3^", TEST_CONFIG[VERSION_FILE]),
      Git("checkout -f hash3 -- %s" % TEST_CONFIG[VERSION_FILE], "",
          cb=ResetVersion(21, 2)),
      Git("log -1 --format=%B hash3", ""),
      Git("log -1 --format=%s hash3", ""),
      Git("svn find-rev hash3", "123"),
      Git("log -1 --format=%ci hash3", "03:15"),
      Git("checkout -f HEAD -- %s" % TEST_CONFIG[VERSION_FILE], "",
          cb=ResetVersion(22, 5)),
      Git("reset --hard svn/trunk", ""),
      Git("log --format=%H", "hash6\n"),
      Git("diff --name-only hash6 hash6^", TEST_CONFIG[VERSION_FILE]),
      Git("checkout -f hash6 -- %s" % TEST_CONFIG[VERSION_FILE], "",
          cb=ResetVersion(22, 3)),
      Git("log -1 --format=%B hash6", ""),
      Git("log -1 --format=%s hash6", ""),
      Git("svn find-rev hash6", "345"),
      Git("log -1 --format=%ci hash6", ""),
      Git("checkout -f HEAD -- %s" % TEST_CONFIG[VERSION_FILE], "",
          cb=ResetVersion(22, 5)),
      Git("reset --hard svn/bleeding_edge", ""),
      Git("log https://v8.googlecode.com/svn/tags -v --limit 20",
          tag_response_text),
      Git("svn find-rev r22626", "hash_22626"),
      Git("svn find-rev hash_22626", "22626"),
      Git("log -1 --format=%ci hash_22626", "01:23"),
      Git("svn find-rev r22624", "hash_22624"),
      Git("svn find-rev hash_22624", "22624"),
      Git("log -1 --format=%ci hash_22624", "02:34"),
      Git("status -s -uno", ""),
      Git("checkout -f master", ""),
      Git("pull", ""),
      Git("checkout -b %s" % TEST_CONFIG[BRANCHNAME], ""),
      Git("log --format=%H --grep=\"V8\"", "c_hash1\nc_hash2\n"),
      Git("diff --name-only c_hash1 c_hash1^", ""),
      Git("diff --name-only c_hash2 c_hash2^", TEST_CONFIG[DEPS_FILE]),
      Git("checkout -f c_hash2 -- %s" % TEST_CONFIG[DEPS_FILE], "",
          cb=ResetDEPS(345)),
      Git("svn find-rev c_hash2", "4567"),
      Git("checkout -f HEAD -- %s" % TEST_CONFIG[DEPS_FILE], "",
          cb=ResetDEPS(567)),
      Git("branch -r", " weird/123\n  branch-heads/7\n"),
      Git("checkout -f branch-heads/7 -- %s" % TEST_CONFIG[DEPS_FILE], "",
          cb=ResetDEPS(345)),
      Git("checkout -f HEAD -- %s" % TEST_CONFIG[DEPS_FILE], "",
          cb=ResetDEPS(567)),
      Git("checkout -f master", ""),
      Git("branch -D %s" % TEST_CONFIG[BRANCHNAME], ""),
      Git("checkout -f some_branch", ""),
      Git("branch -D %s" % TEST_CONFIG[BRANCHNAME], ""),
    ])

    args = ["-c", TEST_CONFIG[CHROMIUM],
            "--json", json_output,
            "--csv", csv_output,
            "--max-releases", "1"]
    Releases(TEST_CONFIG, self).Run(args)

    # Check expected output.
    csv = ("3.28.41,bleeding_edge,22626,,\r\n"
           "3.28.40,bleeding_edge,22624,,\r\n"
           "3.22.3,trunk,345,4567,\r\n"
           "3.21.2,3.21,123,,\r\n"
           "3.3.1.1,3.3,234,,12\r\n")
    self.assertEquals(csv, FileToText(csv_output))

    expected_json = [
      {"bleeding_edge": "22626", "patches_merged": "", "version": "3.28.41",
       "chromium_revision": "", "branch": "bleeding_edge", "revision": "22626",
       "review_link": "", "date": "01:23", "chromium_branch": "",
       "revision_link": "https://code.google.com/p/v8/source/detail?r=22626"},
      {"bleeding_edge": "22624", "patches_merged": "", "version": "3.28.40",
       "chromium_revision": "", "branch": "bleeding_edge", "revision": "22624",
       "review_link": "", "date": "02:34", "chromium_branch": "",
       "revision_link": "https://code.google.com/p/v8/source/detail?r=22624"},
      {"bleeding_edge": "", "patches_merged": "", "version": "3.22.3",
       "chromium_revision": "4567", "branch": "trunk", "revision": "345",
       "review_link": "", "date": "", "chromium_branch": "7",
       "revision_link": "https://code.google.com/p/v8/source/detail?r=345"},
      {"patches_merged": "", "bleeding_edge": "", "version": "3.21.2",
       "chromium_revision": "", "branch": "3.21", "revision": "123",
       "review_link": "", "date": "03:15", "chromium_branch": "",
       "revision_link": "https://code.google.com/p/v8/source/detail?r=123"},
      {"patches_merged": "12", "bleeding_edge": "", "version": "3.3.1.1",
       "chromium_revision": "", "branch": "3.3", "revision": "234",
       "review_link": "fake.com", "date": "18:15", "chromium_branch": "",
       "revision_link": "https://code.google.com/p/v8/source/detail?r=234"},
    ]
    self.assertEquals(expected_json, json.loads(FileToText(json_output)))


  def testBumpUpVersion(self):
    TEST_CONFIG[VERSION_FILE] = self.MakeEmptyTempFile()
    self.WriteFakeVersionFile()

    def ResetVersion(minor, build, patch=0):
      return lambda: self.WriteFakeVersionFile(minor=minor,
                                               build=build,
                                               patch=patch)

    self.ExpectGit([
      Git("status -s -uno", ""),
      Git("checkout -f bleeding_edge", "", cb=ResetVersion(11, 4)),
      Git("pull", ""),
      Git("branch", ""),
      Git("checkout -f bleeding_edge", ""),
      Git("log -1 --format=%H", "latest_hash"),
      Git("diff --name-only latest_hash latest_hash^", ""),
      Git("checkout -f bleeding_edge", ""),
      Git("log --format=%H --grep=\"^git-svn-id: [^@]*@12345 [A-Za-z0-9-]*$\"",
          "lkgr_hash"),
      Git("checkout -b auto-bump-up-version lkgr_hash", ""),
      Git("checkout -f bleeding_edge", ""),
      Git("branch", ""),
      Git("diff --name-only lkgr_hash lkgr_hash^", ""),
      Git("checkout -f master", "", cb=ResetVersion(11, 5)),
      Git("pull", ""),
      Git("checkout -b auto-bump-up-version bleeding_edge", "",
          cb=ResetVersion(11, 4)),
      Git("commit -am \"[Auto-roll] Bump up version to 3.11.6.0\n\n"
          "TBR=author@chromium.org\"", ""),
      Git("cl upload --send-mail --email \"author@chromium.org\" -f "
          "--bypass-hooks", ""),
      Git("cl dcommit -f --bypass-hooks", ""),
      Git("checkout -f bleeding_edge", ""),
      Git("branch", "auto-bump-up-version\n* bleeding_edge"),
      Git("branch -D auto-bump-up-version", ""),
    ])

    self.ExpectReadURL([
      URL("https://v8-status.appspot.com/lkgr", "12345"),
      URL("https://v8-status.appspot.com/current?format=json",
          "{\"message\": \"Tree is open\"}"),
    ])

    BumpUpVersion(TEST_CONFIG, self).Run(["-a", "author@chromium.org"])

  def testAutoTag(self):
    TEST_CONFIG[VERSION_FILE] = self.MakeEmptyTempFile()
    self.WriteFakeVersionFile()

    def ResetVersion(minor, build, patch=0):
      return lambda: self.WriteFakeVersionFile(minor=minor,
                                               build=build,
                                               patch=patch)

    self.ExpectGit([
      Git("status -s -uno", ""),
      Git("status -s -b -uno", "## some_branch\n"),
      Git("svn fetch", ""),
      Git("branch", "  branch1\n* branch2\n"),
      Git("checkout -f master", ""),
      Git("svn rebase", ""),
      Git("checkout -b %s" % TEST_CONFIG[BRANCHNAME], "",
          cb=ResetVersion(4, 5)),
      Git("branch -r", "svn/tags/3.4.2\nsvn/tags/3.2.1.0\nsvn/branches/3.4"),
      Git("log --format=%H --grep=\"\\[Auto\\-roll\\] Bump up version to\"",
          "hash125\nhash118\nhash111\nhash101"),
      Git("checkout -f hash125 -- %s" % TEST_CONFIG[VERSION_FILE], "",
          cb=ResetVersion(4, 4)),
      Git("checkout -f HEAD -- %s" % TEST_CONFIG[VERSION_FILE], "",
          cb=ResetVersion(4, 5)),
      Git("checkout -f hash118 -- %s" % TEST_CONFIG[VERSION_FILE], "",
          cb=ResetVersion(4, 3)),
      Git("checkout -f HEAD -- %s" % TEST_CONFIG[VERSION_FILE], "",
          cb=ResetVersion(4, 5)),
      Git("checkout -f hash111 -- %s" % TEST_CONFIG[VERSION_FILE], "",
          cb=ResetVersion(4, 2)),
      Git("checkout -f HEAD -- %s" % TEST_CONFIG[VERSION_FILE], "",
          cb=ResetVersion(4, 5)),
      Git("svn find-rev hash118", "118"),
      Git("svn find-rev hash125", "125"),
      Git("svn find-rev r123", "hash123"),
      Git("log -1 --format=%at hash123", "1"),
      Git("reset --hard hash123", ""),
      Git("svn tag 3.4.3 -m \"Tagging version 3.4.3\"", ""),
      Git("checkout -f some_branch", ""),
      Git("branch -D %s" % TEST_CONFIG[BRANCHNAME], ""),
    ])

    self.ExpectReadURL([
      URL("https://v8-status.appspot.com/revisions?format=json",
          "[{\"revision\": \"126\", \"status\": true},"
           "{\"revision\": \"123\", \"status\": true},"
           "{\"revision\": \"112\", \"status\": true}]"),
    ])

    AutoTag(TEST_CONFIG, self).Run(["-a", "author@chromium.org"])

  # Test that we bail out if the last change was a version change.
  def testBumpUpVersionBailout1(self):
    TEST_CONFIG[VERSION_FILE] = self.MakeEmptyTempFile()
    self._state["latest"] = "latest_hash"

    self.ExpectGit([
      Git("diff --name-only latest_hash latest_hash^",
          TEST_CONFIG[VERSION_FILE]),
    ])

    self.assertEquals(1,
        self.RunStep(BumpUpVersion, LastChangeBailout, ["--dry_run"]))

  # Test that we bail out if the lkgr was a version change.
  def testBumpUpVersionBailout2(self):
    TEST_CONFIG[VERSION_FILE] = self.MakeEmptyTempFile()
    self._state["lkgr"] = "lkgr_hash"

    self.ExpectGit([
      Git("diff --name-only lkgr_hash lkgr_hash^", TEST_CONFIG[VERSION_FILE]),
    ])

    self.assertEquals(1,
        self.RunStep(BumpUpVersion, LKGRVersionUpToDateBailout, ["--dry_run"]))

  # Test that we bail out if the last version is already newer than the lkgr's
  # version.
  def testBumpUpVersionBailout3(self):
    TEST_CONFIG[VERSION_FILE] = self.MakeEmptyTempFile()
    self._state["lkgr"] = "lkgr_hash"
    self._state["lkgr_version"] = "3.22.4.0"
    self._state["latest_version"] = "3.22.5.0"

    self.ExpectGit([
      Git("diff --name-only lkgr_hash lkgr_hash^", ""),
    ])

    self.assertEquals(1,
        self.RunStep(BumpUpVersion, LKGRVersionUpToDateBailout, ["--dry_run"]))


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
