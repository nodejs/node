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
from auto_push import LastReleaseBailout
import auto_roll
import common_includes
from common_includes import *
import create_release
from create_release import CreateRelease
import merge_to_branch
from merge_to_branch import *
import push_to_candidates
from push_to_candidates import *
import releases
from releases import Releases
from auto_tag import AutoTag


TEST_CONFIG = {
  "DEFAULT_CWD": None,
  "BRANCHNAME": "test-prepare-push",
  "CANDIDATESBRANCH": "test-candidates-push",
  "PERSISTFILE_BASENAME": "/tmp/test-v8-push-to-candidates-tempfile",
  "CHANGELOG_ENTRY_FILE":
      "/tmp/test-v8-push-to-candidates-tempfile-changelog-entry",
  "PATCH_FILE": "/tmp/test-v8-push-to-candidates-tempfile-patch",
  "COMMITMSG_FILE": "/tmp/test-v8-push-to-candidates-tempfile-commitmsg",
  "CHROMIUM": "/tmp/test-v8-push-to-candidates-tempfile-chromium",
  "SETTINGS_LOCATION": None,
  "ALREADY_MERGING_SENTINEL_FILE":
      "/tmp/test-merge-to-branch-tempfile-already-merging",
  "TEMPORARY_PATCH_FILE": "/tmp/test-merge-to-branch-tempfile-temporary-patch",
}


AUTO_PUSH_ARGS = [
  "-a", "author@chromium.org",
  "-r", "reviewer@chromium.org",
]


class ToplevelTest(unittest.TestCase):
  def testSaniniziteVersionTags(self):
    self.assertEquals("4.8.230", SanitizeVersionTag("4.8.230"))
    self.assertEquals("4.8.230", SanitizeVersionTag("tags/4.8.230"))
    self.assertEquals(None, SanitizeVersionTag("candidate"))

  def testNormalizeVersionTags(self):
    input = ["4.8.230",
              "tags/4.8.230",
              "tags/4.8.224.1",
              "4.8.224.1",
              "4.8.223.1",
              "tags/4.8.223",
              "tags/4.8.231",
              "candidates"]
    expected = ["4.8.230",
                "4.8.230",
                "4.8.224.1",
                "4.8.224.1",
                "4.8.223.1",
                "4.8.223",
                "4.8.231",
                ]
    self.assertEquals(expected, NormalizeVersionTags(input))

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


  def WriteFakeVersionFile(self, major=3, minor=22, build=4, patch=0):
    version_file = os.path.join(TEST_CONFIG["DEFAULT_CWD"], VERSION_FILE)
    if not os.path.exists(os.path.dirname(version_file)):
      os.makedirs(os.path.dirname(version_file))
    with open(version_file, "w") as f:
      f.write("  // Some line...\n")
      f.write("\n")
      f.write("#define V8_MAJOR_VERSION    %s\n" % major)
      f.write("#define V8_MINOR_VERSION    %s\n" % minor)
      f.write("#define V8_BUILD_NUMBER     %s\n" % build)
      f.write("#define V8_PATCH_LEVEL      %s\n" % patch)
      f.write("  // Some line...\n")
      f.write("#define V8_IS_CANDIDATE_VERSION 0\n")

  def WriteFakeWatchlistsFile(self):
    watchlists_file = os.path.join(TEST_CONFIG["DEFAULT_CWD"], WATCHLISTS_FILE)
    if not os.path.exists(os.path.dirname(watchlists_file)):
      os.makedirs(os.path.dirname(watchlists_file))
    with open(watchlists_file, "w") as f:

      content = """
    'merges': [
      # Only enabled on branches created with tools/release/create_release.py
      # 'v8-merges@googlegroups.com',
    ],
"""
      f.write(content)

  def MakeStep(self):
    """Convenience wrapper."""
    options = ScriptsBase(TEST_CONFIG, self, self._state).MakeOptions([])
    return MakeStep(step_class=Step, state=self._state,
                    config=TEST_CONFIG, side_effect_handler=self,
                    options=options)

  def RunStep(self, script=PushToCandidates, step_class=Step, args=None):
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

  def Sleep(self, seconds):
    pass

  def GetDate(self):
    return "1999-07-31"

  def GetUTCStamp(self):
    return "1000000"

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
      Cmd("git checkout -f origin/master", ""),
      Cmd("git fetch", ""),
      Cmd("git branch", "  branch1\n* %s" % TEST_CONFIG["BRANCHNAME"]),
      RL("Y"),
      Cmd("git branch -D %s" % TEST_CONFIG["BRANCHNAME"], ""),
    ])
    self.MakeStep().CommonPrepare()
    self.MakeStep().PrepareBranch()

  def testCommonPrepareNoConfirm(self):
    self.Expect([
      Cmd("git status -s -uno", ""),
      Cmd("git checkout -f origin/master", ""),
      Cmd("git fetch", ""),
      Cmd("git branch", "  branch1\n* %s" % TEST_CONFIG["BRANCHNAME"]),
      RL("n"),
    ])
    self.MakeStep().CommonPrepare()
    self.assertRaises(Exception, self.MakeStep().PrepareBranch)

  def testCommonPrepareDeleteBranchFailure(self):
    self.Expect([
      Cmd("git status -s -uno", ""),
      Cmd("git checkout -f origin/master", ""),
      Cmd("git fetch", ""),
      Cmd("git branch", "  branch1\n* %s" % TEST_CONFIG["BRANCHNAME"]),
      RL("Y"),
      Cmd("git branch -D %s" % TEST_CONFIG["BRANCHNAME"], None),
    ])
    self.MakeStep().CommonPrepare()
    self.assertRaises(Exception, self.MakeStep().PrepareBranch)

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
    args = ["--branch", "candidates", "ab12345"]
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

    self.assertEqual("//\n#define V8_BUILD_NUMBER  3\n",
                     MSub(r"(?<=#define V8_BUILD_NUMBER)(?P<space>\s+)\d*$",
                          r"\g<space>3",
                          "//\n#define V8_BUILD_NUMBER  321\n"))

  def testPreparePushRevision(self):
    # Tests the default push hash used when the --revision option is not set.
    self.Expect([
      Cmd("git log -1 --format=%H HEAD", "push_hash")
    ])

    self.RunStep(PushToCandidates, PreparePushRevision)
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

    self._state["last_push_master"] = "1234"
    self._state["push_hash"] = "push_hash"
    self._state["version"] = "3.22.5"
    self.RunStep(PushToCandidates, PrepareChangeLog)

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

    self.RunStep(PushToCandidates, EditChangeLog)

    self.assertEquals("New\n        Lines",
                      FileToText(TEST_CONFIG["CHANGELOG_ENTRY_FILE"]))

  TAGS = """
4425.0
0.0.0.0
3.9.6
3.22.4
test_tag
"""

  # Version as tag: 3.22.4.0. Version on master: 3.22.6.
  # Make sure that the latest version is 3.22.6.0.
  def testIncrementVersion(self):
    self.Expect([
      Cmd("git fetch origin +refs/tags/*:refs/tags/*", ""),
      Cmd("git tag", self.TAGS),
      Cmd("git checkout -f origin/master -- include/v8-version.h",
          "", cb=lambda: self.WriteFakeVersionFile(3, 22, 6)),
    ])

    self.RunStep(PushToCandidates, IncrementVersion)

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

    self.RunStep(PushToCandidates, SquashCommits)
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

  def _PushToCandidates(self, force=False, manual=False):
    TextToFile("", os.path.join(TEST_CONFIG["DEFAULT_CWD"], ".git"))

    # The version file on master has build level 5, while the version
    # file from candidates has build level 4.
    self.WriteFakeVersionFile(build=5)

    TEST_CONFIG["CHANGELOG_ENTRY_FILE"] = self.MakeEmptyTempFile()
    master_change_log = "2014-03-17: Sentinel\n"
    TextToFile(master_change_log,
               os.path.join(TEST_CONFIG["DEFAULT_CWD"], CHANGELOG_FILE))
    os.environ["EDITOR"] = "vi"

    commit_msg_squashed = """Version 3.22.5 (squashed - based on push_hash)

Log text 1 (issue 321).

Performance and stability improvements on all platforms."""

    commit_msg = """Version 3.22.5 (based on push_hash)

Log text 1 (issue 321).

Performance and stability improvements on all platforms."""

    def ResetChangeLog():
      """On 'git co -b new_branch origin/candidates',
      and 'git checkout -- ChangeLog',
      the ChangLog will be reset to its content on candidates."""
      candidates_change_log = """1999-04-05: Version 3.22.4

        Performance and stability improvements on all platforms.\n"""
      TextToFile(candidates_change_log,
                 os.path.join(TEST_CONFIG["DEFAULT_CWD"], CHANGELOG_FILE))

    def ResetToCandidates():
      ResetChangeLog()
      self.WriteFakeVersionFile()

    def CheckVersionCommit():
      commit = FileToText(TEST_CONFIG["COMMITMSG_FILE"])
      self.assertEquals(commit_msg, commit)
      version = FileToText(
          os.path.join(TEST_CONFIG["DEFAULT_CWD"], VERSION_FILE))
      self.assertTrue(re.search(r"#define V8_MINOR_VERSION\s+22", version))
      self.assertTrue(re.search(r"#define V8_BUILD_NUMBER\s+5", version))
      self.assertFalse(re.search(r"#define V8_BUILD_NUMBER\s+6", version))
      self.assertTrue(re.search(r"#define V8_PATCH_LEVEL\s+0", version))
      self.assertTrue(
          re.search(r"#define V8_IS_CANDIDATE_VERSION\s+0", version))

      # Check that the change log on the candidates branch got correctly
      # modified.
      change_log = FileToText(
          os.path.join(TEST_CONFIG["DEFAULT_CWD"], CHANGELOG_FILE))
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
      Cmd("git checkout -f origin/master", ""),
      Cmd("git fetch", ""),
      Cmd("git branch", "  branch1\n* branch2\n"),
      Cmd("git branch", "  branch1\n* branch2\n"),
      Cmd(("git new-branch %s --upstream origin/master" %
           TEST_CONFIG["BRANCHNAME"]), ""),
      Cmd("git fetch origin +refs/tags/*:refs/tags/*", ""),
      Cmd("git tag", self.TAGS),
      Cmd("git checkout -f origin/master -- include/v8-version.h",
          "", cb=self.WriteFakeVersionFile),
      Cmd("git log -1 --format=%H 3.22.4", "release_hash\n"),
      Cmd("git log -1 --format=%s release_hash",
          "Version 3.22.4 (based on abc3)\n"),
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
      Cmd("git checkout -f origin/master", ""),
      Cmd("git diff origin/candidates push_hash", "patch content\n"),
      Cmd(("git new-branch %s --upstream origin/candidates" %
           TEST_CONFIG["CANDIDATESBRANCH"]), "", cb=ResetToCandidates),
      Cmd("git apply --index --reject \"%s\"" % TEST_CONFIG["PATCH_FILE"], ""),
      Cmd("git checkout -f origin/candidates -- ChangeLog", "",
          cb=ResetChangeLog),
      Cmd("git checkout -f origin/candidates -- include/v8-version.h", "",
          cb=self.WriteFakeVersionFile),
      Cmd("git commit -am \"%s\"" % commit_msg_squashed, ""),
    ]
    if manual:
      expectations.append(RL("Y"))  # Sanity check.
    expectations += [
      Cmd("git cl land -f --bypass-hooks", ""),
      Cmd("git checkout -f master", ""),
      Cmd("git fetch", ""),
      Cmd("git branch -D %s" % TEST_CONFIG["CANDIDATESBRANCH"], ""),
      Cmd(("git new-branch %s --upstream origin/candidates" %
           TEST_CONFIG["CANDIDATESBRANCH"]), "", cb=ResetToCandidates),
      Cmd("git commit -aF \"%s\"" % TEST_CONFIG["COMMITMSG_FILE"], "",
          cb=CheckVersionCommit),
      Cmd("git cl land -f --bypass-hooks", ""),
      Cmd("git fetch", ""),
      Cmd("git log -1 --format=%H --grep="
          "\"Version 3.22.5 (based on push_hash)\""
          " origin/candidates", "hsh_to_tag"),
      Cmd("git tag 3.22.5 hsh_to_tag", ""),
      Cmd("git push origin 3.22.5", ""),
      Cmd("git checkout -f origin/master", ""),
      Cmd("git branch -D %s" % TEST_CONFIG["BRANCHNAME"], ""),
      Cmd("git branch -D %s" % TEST_CONFIG["CANDIDATESBRANCH"], ""),
    ]
    self.Expect(expectations)

    args = ["-a", "author@chromium.org", "--revision", "push_hash"]
    if force: args.append("-f")
    if manual: args.append("-m")
    else: args += ["-r", "reviewer@chromium.org"]
    PushToCandidates(TEST_CONFIG, self).Run(args)

    cl = FileToText(os.path.join(TEST_CONFIG["DEFAULT_CWD"], CHANGELOG_FILE))
    self.assertTrue(re.search(r"^\d\d\d\d\-\d+\-\d+: Version 3\.22\.5", cl))
    self.assertTrue(re.search(r"        Log text 1 \(issue 321\).", cl))
    self.assertTrue(re.search(r"1999\-04\-05: Version 3\.22\.4", cl))

    # Note: The version file is on build number 5 again in the end of this test
    # since the git command that merges to master is mocked out.

  def testPushToCandidatesManual(self):
    self._PushToCandidates(manual=True)

  def testPushToCandidatesSemiAutomatic(self):
    self._PushToCandidates()

  def testPushToCandidatesForced(self):
    self._PushToCandidates(force=True)

  def testCreateRelease(self):
    TextToFile("", os.path.join(TEST_CONFIG["DEFAULT_CWD"], ".git"))

    # The version file on master has build level 5.
    self.WriteFakeVersionFile(build=5)

    master_change_log = "2014-03-17: Sentinel\n"
    TextToFile(master_change_log,
               os.path.join(TEST_CONFIG["DEFAULT_CWD"], CHANGELOG_FILE))

    commit_msg = """Version 3.22.5

Log text 1 (issue 321).

Performance and stability improvements on all platforms."""

    def ResetChangeLog():
      last_change_log = """1999-04-05: Version 3.22.4

        Performance and stability improvements on all platforms.\n"""
      TextToFile(last_change_log,
                 os.path.join(TEST_CONFIG["DEFAULT_CWD"], CHANGELOG_FILE))


    def CheckVersionCommit():
      commit = FileToText(TEST_CONFIG["COMMITMSG_FILE"])
      self.assertEquals(commit_msg, commit)
      version = FileToText(
          os.path.join(TEST_CONFIG["DEFAULT_CWD"], VERSION_FILE))
      self.assertTrue(re.search(r"#define V8_MINOR_VERSION\s+22", version))
      self.assertTrue(re.search(r"#define V8_BUILD_NUMBER\s+5", version))
      self.assertFalse(re.search(r"#define V8_BUILD_NUMBER\s+6", version))
      self.assertTrue(re.search(r"#define V8_PATCH_LEVEL\s+0", version))
      self.assertTrue(
          re.search(r"#define V8_IS_CANDIDATE_VERSION\s+0", version))

      # Check that the change log on the candidates branch got correctly
      # modified.
      change_log = FileToText(
          os.path.join(TEST_CONFIG["DEFAULT_CWD"], CHANGELOG_FILE))
      self.assertEquals(
"""1999-07-31: Version 3.22.5

        Log text 1 (issue 321).

        Performance and stability improvements on all platforms.


1999-04-05: Version 3.22.4

        Performance and stability improvements on all platforms.\n""",
          change_log)

    expectations = [
      Cmd("git fetch origin "
          "+refs/heads/*:refs/heads/* "
          "+refs/pending/*:refs/pending/* "
          "+refs/pending-tags/*:refs/pending-tags/*", ""),
      Cmd("git checkout -f origin/master", ""),
      Cmd("git branch", ""),
      Cmd("git fetch origin +refs/tags/*:refs/tags/*", ""),
      Cmd("git tag", self.TAGS),
      Cmd("git checkout -f origin/master -- include/v8-version.h",
          "", cb=self.WriteFakeVersionFile),
      Cmd("git log -1 --format=%H 3.22.4", "release_hash\n"),
      Cmd("git log -1 --format=%s release_hash", "Version 3.22.4\n"),
      Cmd("git log -1 --format=%H release_hash^", "abc3\n"),
      Cmd("git log --format=%H abc3..push_hash", "rev1\n"),
      Cmd("git log -1 --format=%s rev1", "Log text 1.\n"),
      Cmd("git log -1 --format=%B rev1", "Text\nLOG=YES\nBUG=v8:321\nText\n"),
      Cmd("git log -1 --format=%an rev1", "author1@chromium.org\n"),
      Cmd("git reset --hard origin/master", ""),
      Cmd("git checkout -b work-branch push_hash", ""),
      Cmd("git checkout -f 3.22.4 -- ChangeLog", "", cb=ResetChangeLog),
      Cmd("git checkout -f 3.22.4 -- include/v8-version.h", "",
          cb=self.WriteFakeVersionFile),
      Cmd("git checkout -f 3.22.4 -- WATCHLISTS", "",
          cb=self.WriteFakeWatchlistsFile),
      Cmd("git commit -aF \"%s\"" % TEST_CONFIG["COMMITMSG_FILE"], "",
          cb=CheckVersionCommit),
      Cmd("git log -1 --format=%H --grep=\"Version 3.22.5\" origin/3.22.5",
          ""),
      Cmd("git push origin "
          "refs/heads/work-branch:refs/pending/heads/3.22.5 "
          "push_hash:refs/pending-tags/heads/3.22.5 "
          "push_hash:refs/heads/3.22.5", ""),
      Cmd("git fetch", ""),
      Cmd("git log -1 --format=%H --grep="
          "\"Version 3.22.5\" origin/3.22.5", "hsh_to_tag"),
      Cmd("git tag 3.22.5 hsh_to_tag", ""),
      Cmd("git push origin 3.22.5", ""),
      Cmd("git checkout -f origin/master", ""),
      Cmd("git branch", "* master\n  work-branch\n"),
      Cmd("git branch -D work-branch", ""),
      Cmd("git gc", ""),
    ]
    self.Expect(expectations)

    args = ["-a", "author@chromium.org",
            "-r", "reviewer@chromium.org",
            "--revision", "push_hash"]
    CreateRelease(TEST_CONFIG, self).Run(args)

    cl = FileToText(os.path.join(TEST_CONFIG["DEFAULT_CWD"], CHANGELOG_FILE))
    self.assertTrue(re.search(r"^\d\d\d\d\-\d+\-\d+: Version 3\.22\.5", cl))
    self.assertTrue(re.search(r"        Log text 1 \(issue 321\).", cl))
    self.assertTrue(re.search(r"1999\-04\-05: Version 3\.22\.4", cl))

    # Note: The version file is on build number 5 again in the end of this test
    # since the git command that merges to master is mocked out.

    # Check for correct content of the WATCHLISTS file

    watchlists_content = FileToText(os.path.join(TEST_CONFIG["DEFAULT_CWD"],
                                          WATCHLISTS_FILE))
    expected_watchlists_content = """
    'merges': [
      # Only enabled on branches created with tools/release/create_release.py
      'v8-merges@googlegroups.com',
    ],
"""
    self.assertEqual(watchlists_content, expected_watchlists_content)

  C_V8_22624_LOG = """V8 CL.

git-svn-id: https://v8.googlecode.com/svn/branches/bleeding_edge@22624 123

"""

  C_V8_123455_LOG = """V8 CL.

git-svn-id: https://v8.googlecode.com/svn/branches/bleeding_edge@123455 123

"""

  C_V8_123456_LOG = """V8 CL.

git-svn-id: https://v8.googlecode.com/svn/branches/bleeding_edge@123456 123

"""

  ROLL_COMMIT_MSG = """Update V8 to version 3.22.4.

Summary of changes available at:
https://chromium.googlesource.com/v8/v8/+log/last_rol..roll_hsh

Please follow these instructions for assigning/CC'ing issues:
https://github.com/v8/v8/wiki/Triaging%20issues

Please close rolling in case of a roll revert:
https://v8-roll.appspot.com/
This only works with a Google account.

CQ_INCLUDE_TRYBOTS=tryserver.blink:linux_blink_rel

TBR=reviewer@chromium.org"""

  # Snippet from the original DEPS file.
  FAKE_DEPS = """
vars = {
  "v8_revision": "last_roll_hsh",
}
deps = {
  "src/v8":
    (Var("googlecode_url") % "v8") + "/" + Var("v8_branch") + "@" +
    Var("v8_revision"),
}
"""

  def testChromiumRollUpToDate(self):
    TEST_CONFIG["CHROMIUM"] = self.MakeEmptyTempDirectory()
    json_output_file = os.path.join(TEST_CONFIG["CHROMIUM"], "out.json")
    TextToFile(self.FAKE_DEPS, os.path.join(TEST_CONFIG["CHROMIUM"], "DEPS"))
    self.Expect([
      Cmd("git fetch origin", ""),
      Cmd("git fetch origin +refs/tags/*:refs/tags/*", ""),
      Cmd("git describe --tags last_roll_hsh", "3.22.4"),
      Cmd("git fetch origin +refs/tags/*:refs/tags/*", ""),
      Cmd("git rev-list --max-age=395200 --tags",
          "bad_tag\nroll_hsh\nhash_123"),
      Cmd("git describe --tags bad_tag", ""),
      Cmd("git describe --tags roll_hsh", "3.22.4"),
      Cmd("git describe --tags hash_123", "3.22.3"),
      Cmd("git describe --tags roll_hsh", "3.22.4"),
      Cmd("git describe --tags hash_123", "3.22.3"),
    ])

    result = auto_roll.AutoRoll(TEST_CONFIG, self).Run(
        AUTO_PUSH_ARGS + [
          "-c", TEST_CONFIG["CHROMIUM"],
          "--json-output", json_output_file])
    self.assertEquals(0, result)
    json_output = json.loads(FileToText(json_output_file))
    self.assertEquals("up_to_date", json_output["monitoring_state"])


  def testChromiumRoll(self):
    # Setup fake directory structures.
    TEST_CONFIG["CHROMIUM"] = self.MakeEmptyTempDirectory()
    json_output_file = os.path.join(TEST_CONFIG["CHROMIUM"], "out.json")
    TextToFile(self.FAKE_DEPS, os.path.join(TEST_CONFIG["CHROMIUM"], "DEPS"))
    TextToFile("", os.path.join(TEST_CONFIG["CHROMIUM"], ".git"))
    chrome_dir = TEST_CONFIG["CHROMIUM"]
    os.makedirs(os.path.join(chrome_dir, "v8"))

    def WriteDeps():
      TextToFile("Some line\n   \"v8_revision\": \"22624\",\n  some line",
                 os.path.join(chrome_dir, "DEPS"))

    expectations = [
      Cmd("git fetch origin", ""),
      Cmd("git fetch origin +refs/tags/*:refs/tags/*", ""),
      Cmd("git describe --tags last_roll_hsh", "3.22.3.1"),
      Cmd("git fetch origin +refs/tags/*:refs/tags/*", ""),
      Cmd("git rev-list --max-age=395200 --tags",
          "bad_tag\nroll_hsh\nhash_123"),
      Cmd("git describe --tags bad_tag", ""),
      Cmd("git describe --tags roll_hsh", "3.22.4"),
      Cmd("git describe --tags hash_123", "3.22.3"),
      Cmd("git describe --tags roll_hsh", "3.22.4"),
      Cmd("git log -1 --format=%s roll_hsh", "Version 3.22.4\n"),
      Cmd("git describe --tags roll_hsh", "3.22.4"),
      Cmd("git describe --tags last_roll_hsh", "3.22.2.1"),
      Cmd("git status -s -uno", "", cwd=chrome_dir),
      Cmd("git checkout -f master", "", cwd=chrome_dir),
      Cmd("git branch", "", cwd=chrome_dir),
      Cmd("git pull", "", cwd=chrome_dir),
      Cmd("git fetch origin", ""),
      Cmd("git new-branch work-branch", "", cwd=chrome_dir),
      Cmd("roll-dep-svn v8 roll_hsh", "rolled", cb=WriteDeps, cwd=chrome_dir),
      Cmd(("git commit -am \"%s\" "
           "--author \"author@chromium.org <author@chromium.org>\"" %
           self.ROLL_COMMIT_MSG),
          "", cwd=chrome_dir),
      Cmd("git cl upload --send-mail --email \"author@chromium.org\" -f "
          "--use-commit-queue --bypass-hooks", "", cwd=chrome_dir),
      Cmd("git checkout -f master", "", cwd=chrome_dir),
      Cmd("git branch -D work-branch", "", cwd=chrome_dir),
    ]
    self.Expect(expectations)

    args = ["-a", "author@chromium.org", "-c", chrome_dir,
            "-r", "reviewer@chromium.org", "--json-output", json_output_file]
    auto_roll.AutoRoll(TEST_CONFIG, self).Run(args)

    deps = FileToText(os.path.join(chrome_dir, "DEPS"))
    self.assertTrue(re.search("\"v8_revision\": \"22624\"", deps))

    json_output = json.loads(FileToText(json_output_file))
    self.assertEquals("success", json_output["monitoring_state"])

  def testCheckLastPushRecently(self):
    self.Expect([
      Cmd("git fetch origin +refs/tags/*:refs/tags/*", ""),
      Cmd("git tag", self.TAGS),
      Cmd("git log -1 --format=%H 3.22.4", "release_hash\n"),
      Cmd("git log -1 --format=%s release_hash",
          "Version 3.22.4 (based on abc3)\n"),
      Cmd("git log --format=%H abc3..abc123", "\n"),
    ])

    self._state["candidate"] = "abc123"
    self.assertEquals(0, self.RunStep(
        auto_push.AutoPush, LastReleaseBailout, AUTO_PUSH_ARGS))

  def testAutoPush(self):
    self.Expect([
      Cmd("git fetch", ""),
      Cmd("git fetch origin +refs/heads/lkgr:refs/heads/lkgr", ""),
      Cmd("git show-ref -s refs/heads/lkgr", "abc123\n"),
      Cmd("git fetch origin +refs/tags/*:refs/tags/*", ""),
      Cmd("git tag", self.TAGS),
      Cmd("git log -1 --format=%H 3.22.4", "release_hash\n"),
      Cmd("git log -1 --format=%s release_hash",
          "Version 3.22.4 (based on abc3)\n"),
      Cmd("git log --format=%H abc3..abc123", "some_stuff\n"),
    ])

    auto_push.AutoPush(TEST_CONFIG, self).Run(AUTO_PUSH_ARGS + ["--push"])

    state = json.loads(FileToText("%s-state.json"
                                  % TEST_CONFIG["PERSISTFILE_BASENAME"]))

    self.assertEquals("abc123", state["candidate"])

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

    def VerifyLand():
      commit = FileToText(TEST_CONFIG["COMMITMSG_FILE"])
      self.assertEquals(msg, commit)
      version = FileToText(
          os.path.join(TEST_CONFIG["DEFAULT_CWD"], VERSION_FILE))
      self.assertTrue(re.search(r"#define V8_MINOR_VERSION\s+22", version))
      self.assertTrue(re.search(r"#define V8_BUILD_NUMBER\s+5", version))
      self.assertTrue(re.search(r"#define V8_PATCH_LEVEL\s+1", version))
      self.assertTrue(
          re.search(r"#define V8_IS_CANDIDATE_VERSION\s+0", version))

    self.Expect([
      Cmd("git status -s -uno", ""),
      Cmd("git checkout -f origin/master", ""),
      Cmd("git fetch", ""),
      Cmd("git branch", "  branch1\n* branch2\n"),
      Cmd("git new-branch %s --upstream refs/remotes/origin/candidates" %
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
      Cmd("git cl land -f --bypass-hooks", "Closing issue\n",
          cb=VerifyLand),
      Cmd("git fetch", ""),
      Cmd("git log -1 --format=%H --grep=\""
          "Version 3.22.5.1 (cherry-pick)"
          "\" refs/remotes/origin/candidates",
          ""),
      Cmd("git fetch", ""),
      Cmd("git log -1 --format=%H --grep=\""
          "Version 3.22.5.1 (cherry-pick)"
          "\" refs/remotes/origin/candidates",
          "hsh_to_tag"),
      Cmd("git tag 3.22.5.1 hsh_to_tag", ""),
      Cmd("git push origin 3.22.5.1", ""),
      Cmd("git checkout -f origin/master", ""),
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
    c_hash1_commit_log = """Update V8 to Version 4.2.71.

Cr-Commit-Position: refs/heads/master@{#5678}
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
    c_hash_234_commit_log = """Version 3.3.1.1 (cherry-pick).

Merged abc12.

Review URL: fake.com

Cr-Commit-Position: refs/heads/candidates@{#234}
"""
    c_hash_123_commit_log = """Version 3.3.1.0

git-svn-id: googlecode@123 0039-1c4b
"""
    c_hash_345_commit_log = """Version 3.4.0.

Cr-Commit-Position: refs/heads/candidates@{#345}
"""
    c_hash_456_commit_log = """Version 4.2.71.

Cr-Commit-Position: refs/heads/4.2.71@{#1}
"""
    c_deps = "Line\n   \"v8_revision\": \"%s\",\n  line\n"

    json_output = self.MakeEmptyTempFile()
    csv_output = self.MakeEmptyTempFile()
    self.WriteFakeVersionFile()

    TEST_CONFIG["CHROMIUM"] = self.MakeEmptyTempDirectory()
    chrome_dir = TEST_CONFIG["CHROMIUM"]
    chrome_v8_dir = os.path.join(chrome_dir, "v8")
    os.makedirs(chrome_v8_dir)

    def ResetVersion(major, minor, build, patch=0):
      return lambda: self.WriteFakeVersionFile(major=major,
                                               minor=minor,
                                               build=build,
                                               patch=patch)

    self.Expect([
      Cmd("git status -s -uno", ""),
      Cmd("git checkout -f origin/master", ""),
      Cmd("git fetch", ""),
      Cmd("git branch", "  branch1\n* branch2\n"),
      Cmd("git new-branch %s" % TEST_CONFIG["BRANCHNAME"], ""),
      Cmd("git fetch origin +refs/tags/*:refs/tags/*", ""),
      Cmd("git rev-list --max-age=395200 --tags",
          "bad_tag\nhash_234\nhash_123\nhash_345\nhash_456\n"),
      Cmd("git describe --tags bad_tag", "3.23.42-1-deadbeef"),
      Cmd("git describe --tags hash_234", "3.3.1.1"),
      Cmd("git describe --tags hash_123", "3.21.2"),
      Cmd("git describe --tags hash_345", "3.22.3"),
      Cmd("git describe --tags hash_456", "4.2.71"),
      Cmd("git diff --name-only hash_234 hash_234^", VERSION_FILE),
      Cmd("git checkout -f hash_234 -- %s" % VERSION_FILE, "",
          cb=ResetVersion(3, 3, 1, 1)),
      Cmd("git branch -r --contains hash_234", "  branch-heads/3.3\n"),
      Cmd("git log -1 --format=%B hash_234", c_hash_234_commit_log),
      Cmd("git log -1 --format=%s hash_234", ""),
      Cmd("git log -1 --format=%B hash_234", c_hash_234_commit_log),
      Cmd("git log -1 --format=%ci hash_234", "18:15"),
      Cmd("git checkout -f HEAD -- %s" % VERSION_FILE, "",
          cb=ResetVersion(3, 22, 5)),
      Cmd("git diff --name-only hash_123 hash_123^", VERSION_FILE),
      Cmd("git checkout -f hash_123 -- %s" % VERSION_FILE, "",
          cb=ResetVersion(3, 21, 2)),
      Cmd("git branch -r --contains hash_123", "  branch-heads/3.21\n"),
      Cmd("git log -1 --format=%B hash_123", c_hash_123_commit_log),
      Cmd("git log -1 --format=%s hash_123", ""),
      Cmd("git log -1 --format=%B hash_123", c_hash_123_commit_log),
      Cmd("git log -1 --format=%ci hash_123", "03:15"),
      Cmd("git checkout -f HEAD -- %s" % VERSION_FILE, "",
          cb=ResetVersion(3, 22, 5)),
      Cmd("git diff --name-only hash_345 hash_345^", VERSION_FILE),
      Cmd("git checkout -f hash_345 -- %s" % VERSION_FILE, "",
          cb=ResetVersion(3, 22, 3)),
      Cmd("git branch -r --contains hash_345", "  origin/candidates\n"),
      Cmd("git log -1 --format=%B hash_345", c_hash_345_commit_log),
      Cmd("git log -1 --format=%s hash_345", ""),
      Cmd("git log -1 --format=%B hash_345", c_hash_345_commit_log),
      Cmd("git log -1 --format=%ci hash_345", ""),
      Cmd("git checkout -f HEAD -- %s" % VERSION_FILE, "",
          cb=ResetVersion(3, 22, 5)),
      Cmd("git diff --name-only hash_456 hash_456^", VERSION_FILE),
      Cmd("git checkout -f hash_456 -- %s" % VERSION_FILE, "",
          cb=ResetVersion(4, 2, 71)),
      Cmd("git branch -r --contains hash_456", "  origin/4.2.71\n"),
      Cmd("git log -1 --format=%B hash_456", c_hash_456_commit_log),
      Cmd("git log -1 --format=%H 4.2.71", "hash_456"),
      Cmd("git log -1 --format=%s hash_456", "Version 4.2.71"),
      Cmd("git log -1 --format=%H hash_456^", "master_456"),
      Cmd("git log -1 --format=%B master_456",
          "Cr-Commit-Position: refs/heads/master@{#456}"),
      Cmd("git log -1 --format=%B hash_456", c_hash_456_commit_log),
      Cmd("git log -1 --format=%ci hash_456", "02:15"),
      Cmd("git checkout -f HEAD -- %s" % VERSION_FILE, "",
          cb=ResetVersion(3, 22, 5)),
      Cmd("git fetch origin +refs/heads/*:refs/remotes/origin/* "
          "+refs/branch-heads/*:refs/remotes/branch-heads/*", "",
          cwd=chrome_dir),
      Cmd("git fetch origin", "", cwd=chrome_v8_dir),
      Cmd("git log --format=%H --grep=\"V8\" origin/master -- DEPS",
          "c_hash1\nc_hash2\nc_hash3\n",
          cwd=chrome_dir),
      Cmd("git show c_hash1:DEPS", c_deps % "hash_456", cwd=chrome_dir),
      Cmd("git log -1 --format=%B c_hash1", c_hash1_commit_log,
          cwd=chrome_dir),
      Cmd("git show c_hash2:DEPS", c_deps % "hash_345", cwd=chrome_dir),
      Cmd("git log -1 --format=%B c_hash2", c_hash2_commit_log,
          cwd=chrome_dir),
      Cmd("git show c_hash3:DEPS", c_deps % "deadbeef", cwd=chrome_dir),
      Cmd("git log -1 --format=%B c_hash3", c_hash3_commit_log,
          cwd=chrome_dir),
      Cmd("git branch -r", " weird/123\n  branch-heads/7\n", cwd=chrome_dir),
      Cmd("git show refs/branch-heads/7:DEPS", c_deps % "hash_345",
          cwd=chrome_dir),
      URL("http://omahaproxy.appspot.com/all.json", """[{
        "os": "win",
        "versions": [{
          "version": "2.2.2.2",
          "v8_version": "22.2.2.2",
          "current_reldate": "04/09/15",
          "os": "win",
          "channel": "canary",
          "previous_version": "1.1.1.0"
          }]
        }]"""),
      URL("http://omahaproxy.appspot.com/v8.json?version=1.1.1.0", """{
        "chromium_version": "1.1.1.0",
        "v8_version": "11.1.1.0"
        }"""),
      Cmd("git rev-list -1 11.1.1", "v8_previous_version_hash"),
      Cmd("git rev-list -1 22.2.2.2", "v8_version_hash"),
      Cmd("git checkout -f origin/master", ""),
      Cmd("git branch -D %s" % TEST_CONFIG["BRANCHNAME"], "")
    ])

    args = ["-c", TEST_CONFIG["CHROMIUM"],
            "--json", json_output,
            "--csv", csv_output,
            "--max-releases", "1"]
    Releases(TEST_CONFIG, self).Run(args)

    # Check expected output.
    csv = ("4.2.71,4.2.71,1,5678,\r\n"
           "3.22.3,candidates,345,4567:5677,\r\n"
           "3.21.2,3.21,123,,\r\n"
           "3.3.1.1,3.3,234,,abc12\r\n")
    self.assertEquals(csv, FileToText(csv_output))

    expected_json = {"chrome_releases":{
                                        "canaries": [
                                                     {
                           "chrome_version": "2.2.2.2",
                           "os": "win",
                           "release_date": "04/09/15",
                           "v8_version": "22.2.2.2",
                           "v8_version_hash": "v8_version_hash",
                           "v8_previous_version": "11.1.1.0",
                           "v8_previous_version_hash": "v8_previous_version_hash"
                           }]},
                     "releases":[
      {
        "revision": "1",
        "revision_git": "hash_456",
        "master_position": "456",
        "master_hash": "master_456",
        "patches_merged": "",
        "version": "4.2.71",
        "chromium_revision": "5678",
        "branch": "4.2.71",
        "review_link": "",
        "date": "02:15",
        "chromium_branch": "",
        # FIXME(machenbach): Fix revisions link for git.
        "revision_link": "https://code.google.com/p/v8/source/detail?r=1",
      },
      {
        "revision": "345",
        "revision_git": "hash_345",
        "master_position": "",
        "master_hash": "",
        "patches_merged": "",
        "version": "3.22.3",
        "chromium_revision": "4567:5677",
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
        "master_position": "",
        "master_hash": "",
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
        "master_position": "",
        "master_hash": "",
        "version": "3.3.1.1",
        "chromium_revision": "",
        "branch": "3.3",
        "review_link": "fake.com",
        "date": "18:15",
        "chromium_branch": "",
        "revision_link": "https://code.google.com/p/v8/source/detail?r=234",
      },],
    }
    self.assertEquals(expected_json, json.loads(FileToText(json_output)))


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
