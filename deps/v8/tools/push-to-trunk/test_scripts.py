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
import shutil
import tempfile
import traceback
import unittest

import auto_push
from auto_push import CheckLastPush
import auto_roll
import common_includes
from common_includes import *
import merge_to_branch
from merge_to_branch import *
import push_to_trunk
from push_to_trunk import *
import chromium_roll
from chromium_roll import ChromiumRoll
import releases
from releases import Releases
import bump_up_version
from bump_up_version import BumpUpVersion
from bump_up_version import LastChangeBailout
from bump_up_version import LKGRVersionUpToDateBailout
from auto_tag import AutoTag


TEST_CONFIG = {
  "DEFAULT_CWD": None,
  "BRANCHNAME": "test-prepare-push",
  "TRUNKBRANCH": "test-trunk-push",
  "PERSISTFILE_BASENAME": "/tmp/test-v8-push-to-trunk-tempfile",
  "CHANGELOG_FILE": None,
  "CHANGELOG_ENTRY_FILE": "/tmp/test-v8-push-to-trunk-tempfile-changelog-entry",
  "PATCH_FILE": "/tmp/test-v8-push-to-trunk-tempfile-patch",
  "COMMITMSG_FILE": "/tmp/test-v8-push-to-trunk-tempfile-commitmsg",
  "CHROMIUM": "/tmp/test-v8-push-to-trunk-tempfile-chromium",
  "SETTINGS_LOCATION": None,
  "ALREADY_MERGING_SENTINEL_FILE":
      "/tmp/test-merge-to-branch-tempfile-already-merging",
  "TEMPORARY_PATCH_FILE": "/tmp/test-merge-to-branch-tempfile-temporary-patch",
  "CLUSTERFUZZ_API_KEY_FILE": "/tmp/test-fake-cf-api-key",
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


def Cmd(*args, **kwargs):
  """Convenience function returning a shell command test expectation."""
  return {
    "name": "command",
    "args": args,
    "ret": args[-1],
    "cb": kwargs.get("cb"),
    "cwd": kwargs.get("cwd", TEST_CONFIG["DEFAULT_CWD"]),
  }


def RL(text, cb=None):
  """Convenience function returning a readline test expectation."""
  return {
    "name": "readline",
    "args": [],
    "ret": text,
    "cb": cb,
    "cwd": None,
  }


def URL(*args, **kwargs):
  """Convenience function returning a readurl test expectation."""
  return {
    "name": "readurl",
    "args": args[:-1],
    "ret": args[-1],
    "cb": kwargs.get("cb"),
    "cwd": None,
  }


class SimpleMock(object):
  def __init__(self):
    self._recipe = []
    self._index = -1

  def Expect(self, recipe):
    self._recipe = recipe

  def Call(self, name, *args, **kwargs):  # pragma: no cover
    self._index += 1
    try:
      expected_call = self._recipe[self._index]
    except IndexError:
      raise NoRetryException("Calling %s %s" % (name, " ".join(args)))

    if not isinstance(expected_call, dict):
      raise NoRetryException("Found wrong expectation type for %s %s" %
                             (name, " ".join(args)))

    if expected_call["name"] != name:
      raise NoRetryException("Expected action: %s %s - Actual: %s" %
          (expected_call["name"], expected_call["args"], name))

    # Check if the given working directory matches the expected one.
    if expected_call["cwd"] != kwargs.get("cwd"):
      raise NoRetryException("Expected cwd: %s in %s %s - Actual: %s" %
          (expected_call["cwd"],
           expected_call["name"],
           expected_call["args"],
           kwargs.get("cwd")))

    # The number of arguments in the expectation must match the actual
    # arguments.
    if len(args) > len(expected_call['args']):
      raise NoRetryException("When calling %s with arguments, the "
          "expectations must consist of at least as many arguments." %
          name)

    # Compare expected and actual arguments.
    for (expected_arg, actual_arg) in zip(expected_call['args'], args):
      if expected_arg != actual_arg:
        raise NoRetryException("Expected: %s - Actual: %s" %
                               (expected_arg, actual_arg))

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
      raise NoRetryException("Called mock too seldom: %d vs. %d" %
                             (self._index, len(self._recipe)))


class ScriptTest(unittest.TestCase):
  def MakeEmptyTempFile(self):
    handle, name = tempfile.mkstemp()
    os.close(handle)
    self._tmp_files.append(name)
    return name

  def MakeEmptyTempDirectory(self):
    name = tempfile.mkdtemp()
    self._tmp_files.append(name)
    return name


  def WriteFakeVersionFile(self, minor=22, build=4, patch=0):
    version_file = os.path.join(TEST_CONFIG["DEFAULT_CWD"], VERSION_FILE)
    if not os.path.exists(os.path.dirname(version_file)):
      os.makedirs(os.path.dirname(version_file))
    with open(version_file, "w") as f:
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

  def Call(self, fun, *args, **kwargs):
    print "Calling %s with %s and %s" % (str(fun), str(args), str(kwargs))

  def Command(self, cmd, args="", prefix="", pipe=True, cwd=None):
    print "%s %s" % (cmd, args)
    print "in %s" % cwd
    return self._mock.Call("command", cmd + " " + args, cwd=cwd)

  def ReadLine(self):
    return self._mock.Call("readline")

  def ReadURL(self, url, params):
    if params is not None:
      return self._mock.Call("readurl", url, params)
    else:
      return self._mock.Call("readurl", url)

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

  def Expect(self, *args):
    """Convenience wrapper."""
    self._mock.Expect(*args)

  def setUp(self):
    self._mock = SimpleMock()
    self._tmp_files = []
    self._state = {}
    TEST_CONFIG["DEFAULT_CWD"] = self.MakeEmptyTempDirectory()

  def tearDown(self):
    if os.path.exists(TEST_CONFIG["PERSISTFILE_BASENAME"]):
      shutil.rmtree(TEST_CONFIG["PERSISTFILE_BASENAME"])

    # Clean up temps. Doesn't work automatically.
    for name in self._tmp_files:
      if os.path.isfile(name):
        os.remove(name)
      if os.path.isdir(name):
        shutil.rmtree(name)

    self._mock.AssertFinished()

  def testGitMock(self):
    self.Expect([Cmd("git --version", "git version 1.2.3"),
                 Cmd("git dummy", "")])
    self.assertEquals("git version 1.2.3", self.MakeStep().Git("--version"))
    self.assertEquals("", self.MakeStep().Git("dummy"))

  def testCommonPrepareDefault(self):
    self.Expect([
      Cmd("git status -s -uno", ""),
      Cmd("git status -s -b -uno", "## some_branch"),
      Cmd("git fetch", ""),
      Cmd("git svn fetch", ""),
      Cmd("git branch", "  branch1\n* %s" % TEST_CONFIG["BRANCHNAME"]),
      RL("Y"),
      Cmd("git branch -D %s" % TEST_CONFIG["BRANCHNAME"], ""),
    ])
    self.MakeStep().CommonPrepare()
    self.MakeStep().PrepareBranch()
    self.assertEquals("some_branch", self._state["current_branch"])

  def testCommonPrepareNoConfirm(self):
    self.Expect([
      Cmd("git status -s -uno", ""),
      Cmd("git status -s -b -uno", "## some_branch"),
      Cmd("git fetch", ""),
      Cmd("git svn fetch", ""),
      Cmd("git branch", "  branch1\n* %s" % TEST_CONFIG["BRANCHNAME"]),
      RL("n"),
    ])
    self.MakeStep().CommonPrepare()
    self.assertRaises(Exception, self.MakeStep().PrepareBranch)
    self.assertEquals("some_branch", self._state["current_branch"])

  def testCommonPrepareDeleteBranchFailure(self):
    self.Expect([
      Cmd("git status -s -uno", ""),
      Cmd("git status -s -b -uno", "## some_branch"),
      Cmd("git fetch", ""),
      Cmd("git svn fetch", ""),
      Cmd("git branch", "  branch1\n* %s" % TEST_CONFIG["BRANCHNAME"]),
      RL("Y"),
      Cmd("git branch -D %s" % TEST_CONFIG["BRANCHNAME"], None),
    ])
    self.MakeStep().CommonPrepare()
    self.assertRaises(Exception, self.MakeStep().PrepareBranch)
    self.assertEquals("some_branch", self._state["current_branch"])

  def testInitialEnvironmentChecks(self):
    TextToFile("", os.path.join(TEST_CONFIG["DEFAULT_CWD"], ".git"))
    os.environ["EDITOR"] = "vi"
    self.Expect([
      Cmd("which vi", "/usr/bin/vi"),
    ])
    self.MakeStep().InitialEnvironmentChecks(TEST_CONFIG["DEFAULT_CWD"])

  def testTagTimeout(self):
    self.Expect([
      Cmd("git fetch", ""),
      Cmd("git log -1 --format=%H --grep=\"Title\" origin/candidates", ""),
      Cmd("git fetch", ""),
      Cmd("git log -1 --format=%H --grep=\"Title\" origin/candidates", ""),
      Cmd("git fetch", ""),
      Cmd("git log -1 --format=%H --grep=\"Title\" origin/candidates", ""),
      Cmd("git fetch", ""),
      Cmd("git log -1 --format=%H --grep=\"Title\" origin/candidates", ""),
    ])
    args = ["--branch", "candidates", "--vc-interface", "git_read_svn_write",
            "ab12345"]
    self._state["version"] = "tag_name"
    self._state["commit_title"] = "Title"
    self.assertRaises(Exception,
        lambda: self.RunStep(MergeToBranch, TagRevision, args))

  def testReadAndPersistVersion(self):
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
    self.Expect([
      Cmd("git log -1 --format=%H HEAD", "push_hash")
    ])

    self.RunStep(PushToTrunk, PreparePushRevision)
    self.assertEquals("push_hash", self._state["push_hash"])

  def testPrepareChangeLog(self):
    self.WriteFakeVersionFile()
    TEST_CONFIG["CHANGELOG_ENTRY_FILE"] = self.MakeEmptyTempFile()

    self.Expect([
      Cmd("git log --format=%H 1234..push_hash", "rev1\nrev2\nrev3\nrev4"),
      Cmd("git log -1 --format=%s rev1", "Title text 1"),
      Cmd("git log -1 --format=%B rev1", "Title\n\nBUG=\nLOG=y\n"),
      Cmd("git log -1 --format=%an rev1", "author1@chromium.org"),
      Cmd("git log -1 --format=%s rev2", "Title text 2."),
      Cmd("git log -1 --format=%B rev2", "Title\n\nBUG=123\nLOG= \n"),
      Cmd("git log -1 --format=%an rev2", "author2@chromium.org"),
      Cmd("git log -1 --format=%s rev3", "Title text 3"),
      Cmd("git log -1 --format=%B rev3", "Title\n\nBUG=321\nLOG=true\n"),
      Cmd("git log -1 --format=%an rev3", "author3@chromium.org"),
      Cmd("git log -1 --format=%s rev4", "Title text 4"),
      Cmd("git log -1 --format=%B rev4",
       ("Title\n\nBUG=456\nLOG=Y\n\n"
        "Review URL: https://codereview.chromium.org/9876543210\n")),
      URL("https://codereview.chromium.org/9876543210/description",
          "Title\n\nBUG=456\nLOG=N\n\n"),
      Cmd("git log -1 --format=%an rev4", "author4@chromium.org"),
    ])

    self._state["last_push_bleeding_edge"] = "1234"
    self._state["push_hash"] = "push_hash"
    self._state["version"] = "3.22.5"
    self.RunStep(PushToTrunk, PrepareChangeLog)

    actual_cl = FileToText(TEST_CONFIG["CHANGELOG_ENTRY_FILE"])

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
    TEST_CONFIG["CHANGELOG_ENTRY_FILE"] = self.MakeEmptyTempFile()
    TextToFile("  New  \n\tLines  \n", TEST_CONFIG["CHANGELOG_ENTRY_FILE"])
    os.environ["EDITOR"] = "vi"
    self.Expect([
      RL(""),  # Open editor.
      Cmd("vi %s" % TEST_CONFIG["CHANGELOG_ENTRY_FILE"], ""),
    ])

    self.RunStep(PushToTrunk, EditChangeLog)

    self.assertEquals("New\n        Lines",
                      FileToText(TEST_CONFIG["CHANGELOG_ENTRY_FILE"]))

  # Version on trunk: 3.22.4.0. Version on master (bleeding_edge): 3.22.6.
  # Make sure that the increment is 3.22.7.0.
  def testIncrementVersion(self):
    self.WriteFakeVersionFile()
    self._state["last_push_trunk"] = "hash1"
    self._state["latest_build"] = "6"
    self._state["latest_version"] = "3.22.6.0"

    self.Expect([
      Cmd("git checkout -f hash1 -- src/version.cc", ""),
      Cmd("git checkout -f origin/master -- src/version.cc",
          "", cb=lambda: self.WriteFakeVersionFile(22, 6)),
      RL("Y"),  # Increment build number.
    ])

    self.RunStep(PushToTrunk, IncrementVersion)

    self.assertEquals("3", self._state["new_major"])
    self.assertEquals("22", self._state["new_minor"])
    self.assertEquals("7", self._state["new_build"])
    self.assertEquals("0", self._state["new_patch"])

  def _TestSquashCommits(self, change_log, expected_msg):
    TEST_CONFIG["CHANGELOG_ENTRY_FILE"] = self.MakeEmptyTempFile()
    with open(TEST_CONFIG["CHANGELOG_ENTRY_FILE"], "w") as f:
      f.write(change_log)

    self.Expect([
      Cmd("git diff origin/candidates hash1", "patch content"),
    ])

    self._state["push_hash"] = "hash1"
    self._state["date"] = "1999-11-11"

    self.RunStep(PushToTrunk, SquashCommits)
    self.assertEquals(FileToText(TEST_CONFIG["COMMITMSG_FILE"]), expected_msg)

    patch = FileToText(TEST_CONFIG["PATCH_FILE"])
    self.assertTrue(re.search(r"patch content", patch))

  def testSquashCommitsUnformatted(self):
    change_log = """1999-11-11: Version 3.22.5

        Log text 1.
        Chromium issue 12345

        Performance and stability improvements on all platforms.\n"""
    commit_msg = """Version 3.22.5 (based on hash1)

Log text 1. Chromium issue 12345

Performance and stability improvements on all platforms."""
    self._TestSquashCommits(change_log, commit_msg)

  def testSquashCommitsFormatted(self):
    change_log = """1999-11-11: Version 3.22.5

        Long commit message that fills more than 80 characters (Chromium issue
        12345).

        Performance and stability improvements on all platforms.\n"""
    commit_msg = """Version 3.22.5 (based on hash1)

Long commit message that fills more than 80 characters (Chromium issue 12345).

Performance and stability improvements on all platforms."""
    self._TestSquashCommits(change_log, commit_msg)

  def testSquashCommitsQuotationMarks(self):
    change_log = """Line with "quotation marks".\n"""
    commit_msg = """Line with "quotation marks"."""
    self._TestSquashCommits(change_log, commit_msg)

  def testBootstrapper(self):
    work_dir = self.MakeEmptyTempDirectory()
    class FakeScript(ScriptsBase):
      def _Steps(self):
        return []

    # Use the test configuration without the fake testing default work dir.
    fake_config = dict(TEST_CONFIG)
    del(fake_config["DEFAULT_CWD"])

    self.Expect([
      Cmd("fetch v8", "", cwd=work_dir),
    ])
    FakeScript(fake_config, self).Run(["--work-dir", work_dir])

  def _PushToTrunk(self, force=False, manual=False):
    TextToFile("", os.path.join(TEST_CONFIG["DEFAULT_CWD"], ".git"))

    # The version file on bleeding edge has build level 5, while the version
    # file from trunk has build level 4.
    self.WriteFakeVersionFile(build=5)

    TEST_CONFIG["CHANGELOG_ENTRY_FILE"] = self.MakeEmptyTempFile()
    TEST_CONFIG["CHANGELOG_FILE"] = self.MakeEmptyTempFile()
    bleeding_edge_change_log = "2014-03-17: Sentinel\n"
    TextToFile(bleeding_edge_change_log, TEST_CONFIG["CHANGELOG_FILE"])
    os.environ["EDITOR"] = "vi"

    def ResetChangeLog():
      """On 'git co -b new_branch svn/trunk', and 'git checkout -- ChangeLog',
      the ChangLog will be reset to its content on trunk."""
      trunk_change_log = """1999-04-05: Version 3.22.4

        Performance and stability improvements on all platforms.\n"""
      TextToFile(trunk_change_log, TEST_CONFIG["CHANGELOG_FILE"])

    def ResetToTrunk():
      ResetChangeLog()
      self.WriteFakeVersionFile()

    def CheckSVNCommit():
      commit = FileToText(TEST_CONFIG["COMMITMSG_FILE"])
      self.assertEquals(
"""Version 3.22.5 (based on push_hash)

Log text 1 (issue 321).

Performance and stability improvements on all platforms.""", commit)
      version = FileToText(
          os.path.join(TEST_CONFIG["DEFAULT_CWD"], VERSION_FILE))
      self.assertTrue(re.search(r"#define MINOR_VERSION\s+22", version))
      self.assertTrue(re.search(r"#define BUILD_NUMBER\s+5", version))
      self.assertFalse(re.search(r"#define BUILD_NUMBER\s+6", version))
      self.assertTrue(re.search(r"#define PATCH_LEVEL\s+0", version))
      self.assertTrue(re.search(r"#define IS_CANDIDATE_VERSION\s+0", version))

      # Check that the change log on the trunk branch got correctly modified.
      change_log = FileToText(TEST_CONFIG["CHANGELOG_FILE"])
      self.assertEquals(
"""1999-07-31: Version 3.22.5

        Log text 1 (issue 321).

        Performance and stability improvements on all platforms.


1999-04-05: Version 3.22.4

        Performance and stability improvements on all platforms.\n""",
          change_log)

    force_flag = " -f" if not manual else ""
    expectations = []
    if not force:
      expectations.append(Cmd("which vi", "/usr/bin/vi"))
    expectations += [
      Cmd("git status -s -uno", ""),
      Cmd("git status -s -b -uno", "## some_branch\n"),
      Cmd("git fetch", ""),
      Cmd("git svn fetch", ""),
      Cmd("git branch", "  branch1\n* branch2\n"),
      Cmd("git branch", "  branch1\n* branch2\n"),
      Cmd(("git new-branch %s --upstream origin/master" %
           TEST_CONFIG["BRANCHNAME"]),
          ""),
      Cmd(("git log -1 --format=%H --grep="
           "\"^Version [[:digit:]]*\.[[:digit:]]*\.[[:digit:]]* (based\" "
           "origin/candidates"), "hash2\n"),
      Cmd("git log -1 hash2", "Log message\n"),
    ]
    if manual:
      expectations.append(RL("Y"))  # Confirm last push.
    expectations += [
      Cmd("git log -1 --format=%s hash2",
       "Version 3.4.5 (based on abc3)\n"),
      Cmd("git checkout -f origin/master -- src/version.cc",
          "", cb=self.WriteFakeVersionFile),
      Cmd("git checkout -f hash2 -- src/version.cc", "",
          cb=self.WriteFakeVersionFile),
    ]
    if manual:
      expectations.append(RL(""))  # Increment build number.
    expectations += [
      Cmd("git log --format=%H abc3..push_hash", "rev1\n"),
      Cmd("git log -1 --format=%s rev1", "Log text 1.\n"),
      Cmd("git log -1 --format=%B rev1", "Text\nLOG=YES\nBUG=v8:321\nText\n"),
      Cmd("git log -1 --format=%an rev1", "author1@chromium.org\n"),
    ]
    if manual:
      expectations.append(RL(""))  # Open editor.
    if not force:
      expectations.append(
          Cmd("vi %s" % TEST_CONFIG["CHANGELOG_ENTRY_FILE"], ""))
    expectations += [
      Cmd("git fetch", ""),
      Cmd("git svn fetch", "fetch result\n"),
      Cmd("git checkout -f origin/master", ""),
      Cmd("git diff origin/candidates push_hash", "patch content\n"),
      Cmd(("git new-branch %s --upstream origin/candidates" %
           TEST_CONFIG["TRUNKBRANCH"]), "", cb=ResetToTrunk),
      Cmd("git apply --index --reject \"%s\"" % TEST_CONFIG["PATCH_FILE"], ""),
      Cmd(("git checkout -f origin/candidates -- %s" %
           TEST_CONFIG["CHANGELOG_FILE"]), "",
          cb=ResetChangeLog),
      Cmd("git checkout -f origin/candidates -- src/version.cc", "",
          cb=self.WriteFakeVersionFile),
      Cmd("git commit -aF \"%s\"" % TEST_CONFIG["COMMITMSG_FILE"], "",
          cb=CheckSVNCommit),
    ]
    if manual:
      expectations.append(RL("Y"))  # Sanity check.
    expectations += [
      Cmd("git svn dcommit 2>&1", ""),
      Cmd("git fetch", ""),
      Cmd("git log -1 --format=%H --grep="
          "\"Version 3.22.5 (based on push_hash)\""
          " origin/candidates", "hsh_to_tag"),
      Cmd("git tag 3.22.5 hsh_to_tag", ""),
      Cmd("git push origin 3.22.5", ""),
      Cmd("git checkout -f some_branch", ""),
      Cmd("git branch -D %s" % TEST_CONFIG["BRANCHNAME"], ""),
      Cmd("git branch -D %s" % TEST_CONFIG["TRUNKBRANCH"], ""),
    ]
    self.Expect(expectations)

    args = ["-a", "author@chromium.org", "--revision", "push_hash",
            "--vc-interface", "git_read_svn_write",]
    if force: args.append("-f")
    if manual: args.append("-m")
    else: args += ["-r", "reviewer@chromium.org"]
    PushToTrunk(TEST_CONFIG, self).Run(args)

    cl = FileToText(TEST_CONFIG["CHANGELOG_FILE"])
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

  def testPushToTrunkGit(self):
    svn_root = self.MakeEmptyTempDirectory()
    TextToFile("", os.path.join(TEST_CONFIG["DEFAULT_CWD"], ".git"))

    # The version file on bleeding edge has build level 5, while the version
    # file from trunk has build level 4.
    self.WriteFakeVersionFile(build=5)

    TEST_CONFIG["CHANGELOG_ENTRY_FILE"] = self.MakeEmptyTempFile()
    TEST_CONFIG["CHANGELOG_FILE"] = self.MakeEmptyTempFile()
    bleeding_edge_change_log = "2014-03-17: Sentinel\n"
    TextToFile(bleeding_edge_change_log, TEST_CONFIG["CHANGELOG_FILE"])

    def ResetChangeLog():
      """On 'git co -b new_branch svn/trunk', and 'git checkout -- ChangeLog',
      the ChangLog will be reset to its content on trunk."""
      trunk_change_log = """1999-04-05: Version 3.22.4

        Performance and stability improvements on all platforms.\n"""
      TextToFile(trunk_change_log, TEST_CONFIG["CHANGELOG_FILE"])

    def ResetToTrunk():
      ResetChangeLog()
      self.WriteFakeVersionFile()

    def CheckSVNCommit():
      commit = FileToText(TEST_CONFIG["COMMITMSG_FILE"])
      self.assertEquals(
"""Version 3.22.5 (based on push_hash)

Log text 1 (issue 321).

Performance and stability improvements on all platforms.""", commit)
      version = FileToText(
          os.path.join(TEST_CONFIG["DEFAULT_CWD"], VERSION_FILE))
      self.assertTrue(re.search(r"#define MINOR_VERSION\s+22", version))
      self.assertTrue(re.search(r"#define BUILD_NUMBER\s+5", version))
      self.assertFalse(re.search(r"#define BUILD_NUMBER\s+6", version))
      self.assertTrue(re.search(r"#define PATCH_LEVEL\s+0", version))
      self.assertTrue(re.search(r"#define IS_CANDIDATE_VERSION\s+0", version))

      # Check that the change log on the trunk branch got correctly modified.
      change_log = FileToText(TEST_CONFIG["CHANGELOG_FILE"])
      self.assertEquals(
"""1999-07-31: Version 3.22.5

        Log text 1 (issue 321).

        Performance and stability improvements on all platforms.


1999-04-05: Version 3.22.4

        Performance and stability improvements on all platforms.\n""",
          change_log)

    expectations = [
      Cmd("git status -s -uno", ""),
      Cmd("git status -s -b -uno", "## some_branch\n"),
      Cmd("git fetch", ""),
      Cmd("git branch", "  branch1\n* branch2\n"),
      Cmd("git branch", "  branch1\n* branch2\n"),
      Cmd(("git new-branch %s --upstream origin/master" %
           TEST_CONFIG["BRANCHNAME"]),
          ""),
      Cmd(("git log -1 --format=%H --grep="
           "\"^Version [[:digit:]]*\.[[:digit:]]*\.[[:digit:]]* (based\" "
           "origin/candidates"), "hash2\n"),
      Cmd("git log -1 hash2", "Log message\n"),
      Cmd("git log -1 --format=%s hash2",
       "Version 3.4.5 (based on abc3)\n"),
      Cmd("git checkout -f origin/master -- src/version.cc",
          "", cb=self.WriteFakeVersionFile),
      Cmd("git checkout -f hash2 -- src/version.cc", "",
          cb=self.WriteFakeVersionFile),
      Cmd("git log --format=%H abc3..push_hash", "rev1\n"),
      Cmd("git log -1 --format=%s rev1", "Log text 1.\n"),
      Cmd("git log -1 --format=%B rev1", "Text\nLOG=YES\nBUG=v8:321\nText\n"),
      Cmd("git log -1 --format=%an rev1", "author1@chromium.org\n"),
      Cmd("git fetch", ""),
      Cmd("git checkout -f origin/master", ""),
      Cmd("git diff origin/candidates push_hash", "patch content\n"),
      Cmd(("git new-branch %s --upstream origin/candidates" %
           TEST_CONFIG["TRUNKBRANCH"]), "", cb=ResetToTrunk),
      Cmd("git apply --index --reject \"%s\"" % TEST_CONFIG["PATCH_FILE"], ""),
      Cmd(("git checkout -f origin/candidates -- %s" %
           TEST_CONFIG["CHANGELOG_FILE"]), "",
          cb=ResetChangeLog),
      Cmd("git checkout -f origin/candidates -- src/version.cc", "",
          cb=self.WriteFakeVersionFile),
      Cmd("git commit -aF \"%s\"" % TEST_CONFIG["COMMITMSG_FILE"], "",
          cb=CheckSVNCommit),
      # TODO(machenbach): Change test to pure git after flag day.
      # Cmd("git push origin", ""),
      Cmd("git diff HEAD^ HEAD", "patch content"),
      Cmd("svn update", "", cwd=svn_root),
      Cmd("svn status", "", cwd=svn_root),
      Cmd("patch -d trunk -p1 -i %s" %
          TEST_CONFIG["PATCH_FILE"], "Applied patch...", cwd=svn_root),
      Cmd("svn status", "M       OWNERS\n?       new_file\n!       AUTHORS",
          cwd=svn_root),
      Cmd("svn add --force new_file", "", cwd=svn_root),
      Cmd("svn delete --force AUTHORS", "", cwd=svn_root),
      Cmd("svn commit --non-interactive --username=author@chromium.org "
          "--config-dir=[CONFIG_DIR] "
          "-m \"Version 3.22.5 (based on push_hash)\"",
          "", cwd=svn_root),
      Cmd("git fetch", ""),
      Cmd("git log -1 --format=%H --grep="
          "\"Version 3.22.5 (based on push_hash)\""
          " origin/candidates", "hsh_to_tag"),
      Cmd("git tag 3.22.5 hsh_to_tag", ""),
      Cmd("git push https://chromium.googlesource.com/v8/v8 3.22.5", ""),
      Cmd("git checkout -f some_branch", ""),
      Cmd("git branch -D %s" % TEST_CONFIG["BRANCHNAME"], ""),
      Cmd("git branch -D %s" % TEST_CONFIG["TRUNKBRANCH"], ""),
    ]
    self.Expect(expectations)

    args = ["-a", "author@chromium.org", "--revision", "push_hash",
            "--vc-interface", "git", "-f", "-r", "reviewer@chromium.org",
            "--svn", svn_root, "--svn-config", "[CONFIG_DIR]",
            "--work-dir", TEST_CONFIG["DEFAULT_CWD"]]
    PushToTrunk(TEST_CONFIG, self).Run(args)

    cl = FileToText(TEST_CONFIG["CHANGELOG_FILE"])
    self.assertTrue(re.search(r"^\d\d\d\d\-\d+\-\d+: Version 3\.22\.5", cl))
    self.assertTrue(re.search(r"        Log text 1 \(issue 321\).", cl))
    self.assertTrue(re.search(r"1999\-04\-05: Version 3\.22\.4", cl))

    # Note: The version file is on build number 5 again in the end of this test
    # since the git command that merges to the bleeding edge branch is mocked
    # out.

  C_V8_22624_LOG = """V8 CL.

git-svn-id: https://v8.googlecode.com/svn/branches/bleeding_edge@22624 123

"""

  C_V8_123455_LOG = """V8 CL.

git-svn-id: https://v8.googlecode.com/svn/branches/bleeding_edge@123455 123

"""

  C_V8_123456_LOG = """V8 CL.

git-svn-id: https://v8.googlecode.com/svn/branches/bleeding_edge@123456 123

"""

  def testChromiumRoll(self):
    googlers_mapping_py = "%s-mapping.py" % TEST_CONFIG["PERSISTFILE_BASENAME"]
    with open(googlers_mapping_py, "w") as f:
      f.write("""
def list_to_dict(entries):
  return {"g_name@google.com": "c_name@chromium.org"}
def get_list():
  pass""")

    # Setup fake directory structures.
    TEST_CONFIG["CHROMIUM"] = self.MakeEmptyTempDirectory()
    TextToFile("", os.path.join(TEST_CONFIG["CHROMIUM"], ".git"))
    chrome_dir = TEST_CONFIG["CHROMIUM"]
    os.makedirs(os.path.join(chrome_dir, "v8"))

    # Write fake deps file.
    TextToFile("Some line\n   \"v8_revision\": \"123444\",\n  some line",
               os.path.join(chrome_dir, "DEPS"))
    def WriteDeps():
      TextToFile("Some line\n   \"v8_revision\": \"22624\",\n  some line",
                 os.path.join(chrome_dir, "DEPS"))

    expectations = [
      Cmd("git fetch origin", ""),
      Cmd(("git log -1 --format=%H --grep="
           "\"^Version [[:digit:]]*\.[[:digit:]]*\.[[:digit:]]*\" "
           "origin/candidates"), "push_hash\n"),
      Cmd("git log -1 --format=%s push_hash",
          "Version 3.22.5 (based on bleeding_edge revision r22622)\n"),
      URL("https://chromium-build.appspot.com/p/chromium/sheriff_v8.js",
          "document.write('g_name')"),
      Cmd("git status -s -uno", "", cwd=chrome_dir),
      Cmd("git checkout -f master", "", cwd=chrome_dir),
      Cmd("gclient sync --nohooks", "syncing...", cwd=chrome_dir),
      Cmd("git pull", "", cwd=chrome_dir),
      Cmd("git fetch origin", ""),
      Cmd("git new-branch v8-roll-push_hash", "", cwd=chrome_dir),
      Cmd("roll-dep v8 push_hash", "rolled", cb=WriteDeps, cwd=chrome_dir),
      Cmd(("git commit -am \"Update V8 to version 3.22.5 "
           "(based on bleeding_edge revision r22622).\n\n"
           "Please reply to the V8 sheriff c_name@chromium.org in "
           "case of problems.\n\nTBR=c_name@chromium.org\" "
           "--author \"author@chromium.org <author@chromium.org>\""),
          "", cwd=chrome_dir),
      Cmd("git cl upload --send-mail --email \"author@chromium.org\" -f", "",
          cwd=chrome_dir),
    ]
    self.Expect(expectations)

    args = ["-a", "author@chromium.org", "-c", chrome_dir,
            "--sheriff", "--googlers-mapping", googlers_mapping_py,
            "-r", "reviewer@chromium.org"]
    ChromiumRoll(TEST_CONFIG, self).Run(args)

    deps = FileToText(os.path.join(chrome_dir, "DEPS"))
    self.assertTrue(re.search("\"v8_revision\": \"22624\"", deps))

  def testCheckLastPushRecently(self):
    self.Expect([
      Cmd(("git log -1 --format=%H --grep="
           "\"^Version [[:digit:]]*\.[[:digit:]]*\.[[:digit:]]* (based\" "
           "origin/candidates"), "hash2\n"),
      Cmd("git log -1 --format=%s hash2",
          "Version 3.4.5 (based on abc123)\n"),
    ])

    self._state["lkgr"] = "abc123"
    self.assertEquals(0, self.RunStep(
        auto_push.AutoPush, CheckLastPush, AUTO_PUSH_ARGS))

  def testAutoPush(self):
    TextToFile("", os.path.join(TEST_CONFIG["DEFAULT_CWD"], ".git"))
    TEST_CONFIG["SETTINGS_LOCATION"] = "~/.doesnotexist"

    self.Expect([
      Cmd("git status -s -uno", ""),
      Cmd("git status -s -b -uno", "## some_branch\n"),
      Cmd("git fetch", ""),
      URL("https://v8-status.appspot.com/current?format=json",
          "{\"message\": \"Tree is throttled\"}"),
      URL("https://v8-status.appspot.com/lkgr", Exception("Network problem")),
      URL("https://v8-status.appspot.com/lkgr", "abc123"),
      Cmd(("git log -1 --format=%H --grep=\""
           "^Version [[:digit:]]*\.[[:digit:]]*\.[[:digit:]]* (based\""
           " origin/candidates"), "push_hash\n"),
      Cmd("git log -1 --format=%s push_hash",
          "Version 3.4.5 (based on abc101)\n"),
    ])

    auto_push.AutoPush(TEST_CONFIG, self).Run(
        AUTO_PUSH_ARGS + ["--push", "--vc-interface", "git"])

    state = json.loads(FileToText("%s-state.json"
                                  % TEST_CONFIG["PERSISTFILE_BASENAME"]))

    self.assertEquals("abc123", state["lkgr"])

  def testAutoPushStoppedBySettings(self):
    TextToFile("", os.path.join(TEST_CONFIG["DEFAULT_CWD"], ".git"))
    TEST_CONFIG["SETTINGS_LOCATION"] = self.MakeEmptyTempFile()
    TextToFile("{\"enable_auto_push\": false}",
               TEST_CONFIG["SETTINGS_LOCATION"])

    self.Expect([
      Cmd("git status -s -uno", ""),
      Cmd("git status -s -b -uno", "## some_branch\n"),
      Cmd("git fetch", ""),
      Cmd("git svn fetch", ""),
    ])

    def RunAutoPush():
      auto_push.AutoPush(TEST_CONFIG, self).Run(AUTO_PUSH_ARGS)
    self.assertRaises(Exception, RunAutoPush)

  def testAutoPushStoppedByTreeStatus(self):
    TextToFile("", os.path.join(TEST_CONFIG["DEFAULT_CWD"], ".git"))
    TEST_CONFIG["SETTINGS_LOCATION"] = "~/.doesnotexist"

    self.Expect([
      Cmd("git status -s -uno", ""),
      Cmd("git status -s -b -uno", "## some_branch\n"),
      Cmd("git fetch", ""),
      Cmd("git svn fetch", ""),
      URL("https://v8-status.appspot.com/current?format=json",
          "{\"message\": \"Tree is throttled (no push)\"}"),
    ])

    def RunAutoPush():
      auto_push.AutoPush(TEST_CONFIG, self).Run(AUTO_PUSH_ARGS)
    self.assertRaises(Exception, RunAutoPush)

  def testAutoRollExistingRoll(self):
    self.Expect([
      URL("https://codereview.chromium.org/search",
          "owner=author%40chromium.org&limit=30&closed=3&format=json",
          ("{\"results\": [{\"subject\": \"different\"},"
           "{\"subject\": \"Update V8 to Version...\"}]}")),
    ])

    result = auto_roll.AutoRoll(TEST_CONFIG, self).Run(
        AUTO_PUSH_ARGS + ["-c", TEST_CONFIG["CHROMIUM"]])
    self.assertEquals(0, result)

  # Snippet from the original DEPS file.
  FAKE_DEPS = """
vars = {
  "v8_revision": "abcd123455",
}
deps = {
  "src/v8":
    (Var("googlecode_url") % "v8") + "/" + Var("v8_branch") + "@" +
    Var("v8_revision"),
}
"""

  def testAutoRollUpToDate(self):
    TEST_CONFIG["CHROMIUM"] = self.MakeEmptyTempDirectory()
    TextToFile(self.FAKE_DEPS, os.path.join(TEST_CONFIG["CHROMIUM"], "DEPS"))
    self.Expect([
      URL("https://codereview.chromium.org/search",
          "owner=author%40chromium.org&limit=30&closed=3&format=json",
          ("{\"results\": [{\"subject\": \"different\"}]}")),
      Cmd("git fetch", ""),
      Cmd("git svn fetch", ""),
      Cmd(("git log -1 --format=%H --grep="
           "\"^Version [[:digit:]]*\.[[:digit:]]*\.[[:digit:]]*\" "
           "origin/candidates"), "push_hash\n"),
      Cmd("git log -1 --format=%B push_hash", self.C_V8_22624_LOG),
      Cmd("git log -1 --format=%B abcd123455", self.C_V8_123455_LOG),
    ])

    result = auto_roll.AutoRoll(TEST_CONFIG, self).Run(
        AUTO_PUSH_ARGS + ["-c", TEST_CONFIG["CHROMIUM"]])
    self.assertEquals(0, result)

  def testAutoRoll(self):
    TEST_CONFIG["CHROMIUM"] = self.MakeEmptyTempDirectory()
    TextToFile(self.FAKE_DEPS, os.path.join(TEST_CONFIG["CHROMIUM"], "DEPS"))
    TEST_CONFIG["CLUSTERFUZZ_API_KEY_FILE"]  = self.MakeEmptyTempFile()
    TextToFile("fake key", TEST_CONFIG["CLUSTERFUZZ_API_KEY_FILE"])

    self.Expect([
      URL("https://codereview.chromium.org/search",
          "owner=author%40chromium.org&limit=30&closed=3&format=json",
          ("{\"results\": [{\"subject\": \"different\"}]}")),
      Cmd("git fetch", ""),
      Cmd("git svn fetch", ""),
      Cmd(("git log -1 --format=%H --grep="
           "\"^Version [[:digit:]]*\.[[:digit:]]*\.[[:digit:]]*\" "
           "origin/candidates"), "push_hash\n"),
      Cmd("git log -1 --format=%B push_hash", self.C_V8_123456_LOG),
      Cmd("git log -1 --format=%B abcd123455", self.C_V8_123455_LOG),
    ])

    result = auto_roll.AutoRoll(TEST_CONFIG, self).Run(
        AUTO_PUSH_ARGS + ["-c", TEST_CONFIG["CHROMIUM"], "--roll"])
    self.assertEquals(0, result)

  def testMergeToBranch(self):
    TEST_CONFIG["ALREADY_MERGING_SENTINEL_FILE"] = self.MakeEmptyTempFile()
    TextToFile("", os.path.join(TEST_CONFIG["DEFAULT_CWD"], ".git"))
    self.WriteFakeVersionFile(build=5)
    os.environ["EDITOR"] = "vi"
    extra_patch = self.MakeEmptyTempFile()

    def VerifyPatch(patch):
      return lambda: self.assertEquals(patch,
          FileToText(TEST_CONFIG["TEMPORARY_PATCH_FILE"]))

    msg = """Version 3.22.5.1 (cherry-pick)

Merged ab12345
Merged ab23456
Merged ab34567
Merged ab45678
Merged ab56789

Title4

Title2

Title3

Title1

Revert "Something"

BUG=123,234,345,456,567,v8:123
LOG=N
"""

    def VerifySVNCommit():
      commit = FileToText(TEST_CONFIG["COMMITMSG_FILE"])
      self.assertEquals(msg, commit)
      version = FileToText(
          os.path.join(TEST_CONFIG["DEFAULT_CWD"], VERSION_FILE))
      self.assertTrue(re.search(r"#define MINOR_VERSION\s+22", version))
      self.assertTrue(re.search(r"#define BUILD_NUMBER\s+5", version))
      self.assertTrue(re.search(r"#define PATCH_LEVEL\s+1", version))
      self.assertTrue(re.search(r"#define IS_CANDIDATE_VERSION\s+0", version))

    self.Expect([
      Cmd("git status -s -uno", ""),
      Cmd("git status -s -b -uno", "## some_branch\n"),
      Cmd("git fetch", ""),
      Cmd("git svn fetch", ""),
      Cmd("git branch", "  branch1\n* branch2\n"),
      Cmd("git new-branch %s --upstream origin/candidates" %
          TEST_CONFIG["BRANCHNAME"], ""),
      Cmd(("git log --format=%H --grep=\"Port ab12345\" "
           "--reverse origin/master"),
          "ab45678\nab23456"),
      Cmd("git log -1 --format=%s ab45678", "Title1"),
      Cmd("git log -1 --format=%s ab23456", "Title2"),
      Cmd(("git log --format=%H --grep=\"Port ab23456\" "
           "--reverse origin/master"),
          ""),
      Cmd(("git log --format=%H --grep=\"Port ab34567\" "
           "--reverse origin/master"),
          "ab56789"),
      Cmd("git log -1 --format=%s ab56789", "Title3"),
      RL("Y"),  # Automatically add corresponding ports (ab34567, ab56789)?
      # Simulate git being down which stops the script.
      Cmd("git log -1 --format=%s ab12345", None),
      # Restart script in the failing step.
      Cmd("git log -1 --format=%s ab12345", "Title4"),
      Cmd("git log -1 --format=%s ab23456", "Title2"),
      Cmd("git log -1 --format=%s ab34567", "Title3"),
      Cmd("git log -1 --format=%s ab45678", "Title1"),
      Cmd("git log -1 --format=%s ab56789", "Revert \"Something\""),
      Cmd("git log -1 ab12345", "Title4\nBUG=123\nBUG=234"),
      Cmd("git log -1 ab23456", "Title2\n BUG = v8:123,345"),
      Cmd("git log -1 ab34567", "Title3\nLOG=n\nBUG=567, 456"),
      Cmd("git log -1 ab45678", "Title1\nBUG="),
      Cmd("git log -1 ab56789", "Revert \"Something\"\nBUG=none"),
      Cmd("git log -1 -p ab12345", "patch4"),
      Cmd(("git apply --index --reject \"%s\"" %
           TEST_CONFIG["TEMPORARY_PATCH_FILE"]),
          "", cb=VerifyPatch("patch4")),
      Cmd("git log -1 -p ab23456", "patch2"),
      Cmd(("git apply --index --reject \"%s\"" %
           TEST_CONFIG["TEMPORARY_PATCH_FILE"]),
          "", cb=VerifyPatch("patch2")),
      Cmd("git log -1 -p ab34567", "patch3"),
      Cmd(("git apply --index --reject \"%s\"" %
           TEST_CONFIG["TEMPORARY_PATCH_FILE"]),
          "", cb=VerifyPatch("patch3")),
      Cmd("git log -1 -p ab45678", "patch1"),
      Cmd(("git apply --index --reject \"%s\"" %
           TEST_CONFIG["TEMPORARY_PATCH_FILE"]),
          "", cb=VerifyPatch("patch1")),
      Cmd("git log -1 -p ab56789", "patch5\n"),
      Cmd(("git apply --index --reject \"%s\"" %
           TEST_CONFIG["TEMPORARY_PATCH_FILE"]),
          "", cb=VerifyPatch("patch5\n")),
      Cmd("git apply --index --reject \"%s\"" % extra_patch, ""),
      RL("Y"),  # Automatically increment patch level?
      Cmd("git commit -aF \"%s\"" % TEST_CONFIG["COMMITMSG_FILE"], ""),
      RL("reviewer@chromium.org"),  # V8 reviewer.
      Cmd("git cl upload --send-mail -r \"reviewer@chromium.org\" "
          "--bypass-hooks --cc \"ulan@chromium.org\"", ""),
      Cmd("git checkout -f %s" % TEST_CONFIG["BRANCHNAME"], ""),
      RL("LGTM"),  # Enter LGTM for V8 CL.
      Cmd("git cl presubmit", "Presubmit successfull\n"),
      Cmd("git cl dcommit -f --bypass-hooks", "Closing issue\n",
          cb=VerifySVNCommit),
      Cmd("git fetch", ""),
      Cmd("git log -1 --format=%H --grep=\""
          "Version 3.22.5.1 (cherry-pick)"
          "\" origin/candidates",
          ""),
      Cmd("git fetch", ""),
      Cmd("git log -1 --format=%H --grep=\""
          "Version 3.22.5.1 (cherry-pick)"
          "\" origin/candidates",
          "hsh_to_tag"),
      Cmd("git tag 3.22.5.1 hsh_to_tag", ""),
      Cmd("git push origin 3.22.5.1", ""),
      Cmd("git checkout -f some_branch", ""),
      Cmd("git branch -D %s" % TEST_CONFIG["BRANCHNAME"], ""),
    ])

    # ab12345 and ab34567 are patches. ab23456 (included) and ab45678 are the
    # MIPS ports of ab12345. ab56789 is the MIPS port of ab34567.
    args = ["-f", "-p", extra_patch, "--branch", "candidates",
            "ab12345", "ab23456", "ab34567"]

    # The first run of the script stops because of git being down.
    self.assertRaises(GitFailedException,
        lambda: MergeToBranch(TEST_CONFIG, self).Run(args))

    # Test that state recovery after restarting the script works.
    args += ["-s", "4"]
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
    c_hash2_commit_log = """Revert something.

BUG=12345

Reason:
> Some reason.
> Cr-Commit-Position: refs/heads/master@{#12345}
> git-svn-id: svn://svn.chromium.org/chrome/trunk/src@12345 003-1c4

Review URL: https://codereview.chromium.org/12345

Cr-Commit-Position: refs/heads/master@{#4567}
git-svn-id: svn://svn.chromium.org/chrome/trunk/src@4567 0039-1c4b

"""
    c_hash3_commit_log = """Simple.

git-svn-id: svn://svn.chromium.org/chrome/trunk/src@3456 0039-1c4b

"""
    json_output = self.MakeEmptyTempFile()
    csv_output = self.MakeEmptyTempFile()
    self.WriteFakeVersionFile()

    TEST_CONFIG["CHROMIUM"] = self.MakeEmptyTempDirectory()
    chrome_dir = TEST_CONFIG["CHROMIUM"]
    chrome_v8_dir = os.path.join(chrome_dir, "v8")
    os.makedirs(chrome_v8_dir)
    def WriteDEPS(revision):
      TextToFile("Line\n   \"v8_revision\": \"%s\",\n  line\n" % revision,
                 os.path.join(chrome_dir, "DEPS"))
    WriteDEPS(567)

    def ResetVersion(minor, build, patch=0):
      return lambda: self.WriteFakeVersionFile(minor=minor,
                                               build=build,
                                               patch=patch)

    def ResetDEPS(revision):
      return lambda: WriteDEPS(revision)

    self.Expect([
      Cmd("git status -s -uno", ""),
      Cmd("git status -s -b -uno", "## some_branch\n"),
      Cmd("git fetch", ""),
      Cmd("git svn fetch", ""),
      Cmd("git branch", "  branch1\n* branch2\n"),
      Cmd("git new-branch %s" % TEST_CONFIG["BRANCHNAME"], ""),
      Cmd("git branch -r", "  branch-heads/3.21\n  branch-heads/3.3\n"),
      Cmd("git reset --hard branch-heads/3.3", ""),
      Cmd("git log --format=%H", "hash1\nhash_234"),
      Cmd("git diff --name-only hash1 hash1^", ""),
      Cmd("git diff --name-only hash_234 hash_234^", VERSION_FILE),
      Cmd("git checkout -f hash_234 -- %s" % VERSION_FILE, "",
          cb=ResetVersion(3, 1, 1)),
      Cmd("git log -1 --format=%B hash_234",
          "Version 3.3.1.1 (cherry-pick).\n\n"
          "Merged abc12.\n\n"
          "Review URL: fake.com\n"),
      Cmd("git log -1 --format=%s hash_234", ""),
      Cmd("git svn find-rev hash_234", "234"),
      Cmd("git log -1 --format=%ci hash_234", "18:15"),
      Cmd("git checkout -f HEAD -- %s" % VERSION_FILE, "",
          cb=ResetVersion(22, 5)),
      Cmd("git reset --hard branch-heads/3.21", ""),
      Cmd("git log --format=%H", "hash_123\nhash4\nhash5\n"),
      Cmd("git diff --name-only hash_123 hash_123^", VERSION_FILE),
      Cmd("git checkout -f hash_123 -- %s" % VERSION_FILE, "",
          cb=ResetVersion(21, 2)),
      Cmd("git log -1 --format=%B hash_123", ""),
      Cmd("git log -1 --format=%s hash_123", ""),
      Cmd("git svn find-rev hash_123", "123"),
      Cmd("git log -1 --format=%ci hash_123", "03:15"),
      Cmd("git checkout -f HEAD -- %s" % VERSION_FILE, "",
          cb=ResetVersion(22, 5)),
      Cmd("git reset --hard origin/candidates", ""),
      Cmd("git log --format=%H", "hash_345\n"),
      Cmd("git diff --name-only hash_345 hash_345^", VERSION_FILE),
      Cmd("git checkout -f hash_345 -- %s" % VERSION_FILE, "",
          cb=ResetVersion(22, 3)),
      Cmd("git log -1 --format=%B hash_345", ""),
      Cmd("git log -1 --format=%s hash_345", ""),
      Cmd("git svn find-rev hash_345", "345"),
      Cmd("git log -1 --format=%ci hash_345", ""),
      Cmd("git checkout -f HEAD -- %s" % VERSION_FILE, "",
          cb=ResetVersion(22, 5)),
      Cmd("git reset --hard origin/master", ""),
      Cmd("svn log https://v8.googlecode.com/svn/tags -v --limit 20",
          tag_response_text),
      Cmd("git svn find-rev r22626", "hash_22626"),
      Cmd("git svn find-rev hash_22626", "22626"),
      Cmd("git log -1 --format=%ci hash_22626", "01:23"),
      Cmd("git svn find-rev r22624", "hash_22624"),
      Cmd("git svn find-rev hash_22624", "22624"),
      Cmd("git log -1 --format=%ci hash_22624", "02:34"),
      Cmd("git status -s -uno", "", cwd=chrome_dir),
      Cmd("git checkout -f master", "", cwd=chrome_dir),
      Cmd("git pull", "", cwd=chrome_dir),
      Cmd("git new-branch %s" % TEST_CONFIG["BRANCHNAME"], "",
          cwd=chrome_dir),
      Cmd("git fetch origin", "", cwd=chrome_v8_dir),
      Cmd("git log --format=%H --grep=\"V8\"", "c_hash1\nc_hash2\nc_hash3\n",
          cwd=chrome_dir),
      Cmd("git diff --name-only c_hash1 c_hash1^", "", cwd=chrome_dir),
      Cmd("git diff --name-only c_hash2 c_hash2^", "DEPS", cwd=chrome_dir),
      Cmd("git checkout -f c_hash2 -- DEPS", "",
          cb=ResetDEPS("0123456789012345678901234567890123456789"),
          cwd=chrome_dir),
      Cmd("git log -1 --format=%B c_hash2", c_hash2_commit_log,
          cwd=chrome_dir),
      Cmd("git rev-list -n 1 0123456789012345678901234567890123456789",
          "0123456789012345678901234567890123456789", cwd=chrome_v8_dir),
      Cmd("git log -1 --format=%B 0123456789012345678901234567890123456789",
          self.C_V8_22624_LOG, cwd=chrome_v8_dir),
      Cmd("git diff --name-only c_hash3 c_hash3^", "DEPS", cwd=chrome_dir),
      Cmd("git checkout -f c_hash3 -- DEPS", "", cb=ResetDEPS(345),
          cwd=chrome_dir),
      Cmd("git log -1 --format=%B c_hash3", c_hash3_commit_log,
          cwd=chrome_dir),
      Cmd("git checkout -f HEAD -- DEPS", "", cb=ResetDEPS(567),
          cwd=chrome_dir),
      Cmd("git branch -r", " weird/123\n  branch-heads/7\n", cwd=chrome_dir),
      Cmd("git checkout -f branch-heads/7 -- DEPS", "", cb=ResetDEPS(345),
          cwd=chrome_dir),
      Cmd("git checkout -f HEAD -- DEPS", "", cb=ResetDEPS(567),
          cwd=chrome_dir),
      Cmd("git checkout -f master", "", cwd=chrome_dir),
      Cmd("git branch -D %s" % TEST_CONFIG["BRANCHNAME"], "", cwd=chrome_dir),
      Cmd("git checkout -f some_branch", ""),
      Cmd("git branch -D %s" % TEST_CONFIG["BRANCHNAME"], ""),
    ])

    args = ["-c", TEST_CONFIG["CHROMIUM"],
            "--vc-interface", "git_read_svn_write",
            "--json", json_output,
            "--csv", csv_output,
            "--max-releases", "1"]
    Releases(TEST_CONFIG, self).Run(args)

    # Check expected output.
    csv = ("3.28.41,master,22626,,\r\n"
           "3.28.40,master,22624,4567,\r\n"
           "3.22.3,candidates,345,3456:4566,\r\n"
           "3.21.2,3.21,123,,\r\n"
           "3.3.1.1,3.3,234,,abc12\r\n")
    self.assertEquals(csv, FileToText(csv_output))

    expected_json = [
      {
        "revision": "22626",
        "revision_git": "hash_22626",
        "bleeding_edge": "22626",
        "bleeding_edge_git": "hash_22626",
        "patches_merged": "",
        "version": "3.28.41",
        "chromium_revision": "",
        "branch": "master",
        "review_link": "",
        "date": "01:23",
        "chromium_branch": "",
        "revision_link": "https://code.google.com/p/v8/source/detail?r=22626",
      },
      {
        "revision": "22624",
        "revision_git": "hash_22624",
        "bleeding_edge": "22624",
        "bleeding_edge_git": "hash_22624",
        "patches_merged": "",
        "version": "3.28.40",
        "chromium_revision": "4567",
        "branch": "master",
        "review_link": "",
        "date": "02:34",
        "chromium_branch": "",
        "revision_link": "https://code.google.com/p/v8/source/detail?r=22624",
      },
      {
        "revision": "345",
        "revision_git": "hash_345",
        "bleeding_edge": "",
        "bleeding_edge_git": "",
        "patches_merged": "",
        "version": "3.22.3",
        "chromium_revision": "3456:4566",
        "branch": "candidates",
        "review_link": "",
        "date": "",
        "chromium_branch": "7",
        "revision_link": "https://code.google.com/p/v8/source/detail?r=345",
      },
      {
        "revision": "123",
        "revision_git": "hash_123",
        "patches_merged": "",
        "bleeding_edge": "",
        "bleeding_edge_git": "",
        "version": "3.21.2",
        "chromium_revision": "",
        "branch": "3.21",
        "review_link": "",
        "date": "03:15",
        "chromium_branch": "",
        "revision_link": "https://code.google.com/p/v8/source/detail?r=123",
      },
      {
        "revision": "234",
        "revision_git": "hash_234",
        "patches_merged": "abc12",
        "bleeding_edge": "",
        "bleeding_edge_git": "",
        "version": "3.3.1.1",
        "chromium_revision": "",
        "branch": "3.3",
        "review_link": "fake.com",
        "date": "18:15",
        "chromium_branch": "",
        "revision_link": "https://code.google.com/p/v8/source/detail?r=234",
      },
    ]
    self.assertEquals(expected_json, json.loads(FileToText(json_output)))


  def _bumpUpVersion(self):
    self.WriteFakeVersionFile()

    def ResetVersion(minor, build, patch=0):
      return lambda: self.WriteFakeVersionFile(minor=minor,
                                               build=build,
                                               patch=patch)

    return [
      Cmd("git status -s -uno", ""),
      Cmd("git checkout -f master", "", cb=ResetVersion(11, 4)),
      Cmd("git pull", ""),
      Cmd("git branch", ""),
      Cmd("git checkout -f master", ""),
      Cmd("git log -1 --format=%H", "latest_hash"),
      Cmd("git diff --name-only latest_hash latest_hash^", ""),
      URL("https://v8-status.appspot.com/lkgr", "12345"),
      Cmd("git checkout -f master", ""),
      Cmd(("git log --format=%H --grep="
           "\"^git-svn-id: [^@]*@12345 [A-Za-z0-9-]*$\""),
          "lkgr_hash"),
      Cmd("git new-branch auto-bump-up-version --upstream lkgr_hash", ""),
      Cmd("git checkout -f master", ""),
      Cmd("git branch", "auto-bump-up-version\n* master"),
      Cmd("git branch -D auto-bump-up-version", ""),
      Cmd("git diff --name-only lkgr_hash lkgr_hash^", ""),
      Cmd("git checkout -f candidates", "", cb=ResetVersion(11, 5)),
      Cmd("git pull", ""),
      URL("https://v8-status.appspot.com/current?format=json",
          "{\"message\": \"Tree is open\"}"),
      Cmd("git new-branch auto-bump-up-version --upstream master", "",
          cb=ResetVersion(11, 4)),
      Cmd("git commit -am \"[Auto-roll] Bump up version to 3.11.6.0\n\n"
          "TBR=author@chromium.org\" "
          "--author \"author@chromium.org <author@chromium.org>\"", ""),
    ]

  def testBumpUpVersionGit(self):
    expectations = self._bumpUpVersion()
    expectations += [
      Cmd("git cl upload --send-mail --email \"author@chromium.org\" -f "
          "--bypass-hooks", ""),
      Cmd("git cl dcommit -f --bypass-hooks", ""),
      Cmd("git checkout -f master", ""),
      Cmd("git branch", "auto-bump-up-version\n* master"),
      Cmd("git branch -D auto-bump-up-version", ""),
    ]
    self.Expect(expectations)

    BumpUpVersion(TEST_CONFIG, self).Run(["-a", "author@chromium.org"])

  def testBumpUpVersionSvn(self):
    svn_root = self.MakeEmptyTempDirectory()
    expectations = self._bumpUpVersion()
    expectations += [
      Cmd("git diff HEAD^ HEAD", "patch content"),
      Cmd("svn update", "", cwd=svn_root),
      Cmd("svn status", "", cwd=svn_root),
      Cmd("patch -d branches/bleeding_edge -p1 -i %s" %
          TEST_CONFIG["PATCH_FILE"], "Applied patch...", cwd=svn_root),
     Cmd("svn status", "M       src/version.cc", cwd=svn_root),
      Cmd("svn commit --non-interactive --username=author@chromium.org "
          "--config-dir=[CONFIG_DIR] "
          "-m \"[Auto-roll] Bump up version to 3.11.6.0\"",
          "", cwd=svn_root),
      Cmd("git checkout -f master", ""),
      Cmd("git branch", "auto-bump-up-version\n* master"),
      Cmd("git branch -D auto-bump-up-version", ""),
    ]
    self.Expect(expectations)

    BumpUpVersion(TEST_CONFIG, self).Run(
        ["-a", "author@chromium.org",
         "--svn", svn_root,
         "--svn-config", "[CONFIG_DIR]"])

  # Test that we bail out if the last change was a version change.
  def testBumpUpVersionBailout1(self):
    self._state["latest"] = "latest_hash"

    self.Expect([
      Cmd("git diff --name-only latest_hash latest_hash^", VERSION_FILE),
    ])

    self.assertEquals(0,
        self.RunStep(BumpUpVersion, LastChangeBailout, ["--dry_run"]))

  # Test that we bail out if the lkgr was a version change.
  def testBumpUpVersionBailout2(self):
    self._state["lkgr"] = "lkgr_hash"

    self.Expect([
      Cmd("git diff --name-only lkgr_hash lkgr_hash^", VERSION_FILE),
    ])

    self.assertEquals(0,
        self.RunStep(BumpUpVersion, LKGRVersionUpToDateBailout, ["--dry_run"]))

  # Test that we bail out if the last version is already newer than the lkgr's
  # version.
  def testBumpUpVersionBailout3(self):
    self._state["lkgr"] = "lkgr_hash"
    self._state["lkgr_version"] = "3.22.4.0"
    self._state["latest_version"] = "3.22.5.0"

    self.Expect([
      Cmd("git diff --name-only lkgr_hash lkgr_hash^", ""),
    ])

    self.assertEquals(0,
        self.RunStep(BumpUpVersion, LKGRVersionUpToDateBailout, ["--dry_run"]))


class SystemTest(unittest.TestCase):
  def testReload(self):
    options = ScriptsBase(
        TEST_CONFIG, DEFAULT_SIDE_EFFECT_HANDLER, {}).MakeOptions([])
    step = MakeStep(step_class=PrepareChangeLog, number=0, state={}, config={},
                    options=options,
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
