#!/usr/bin/env python
# Copyright 2014 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from collections import namedtuple
import coverage
import json
from mock import DEFAULT
from mock import MagicMock
import os
from os import path, sys
import shutil
import tempfile
import unittest

# Requires python-coverage and python-mock. Native python coverage
# version >= 3.7.1 should be installed to get the best speed.

TEST_WORKSPACE = path.join(tempfile.gettempdir(), "test-v8-run-perf")

V8_JSON = {
  "path": ["."],
  "binary": "d7",
  "flags": ["--flag"],
  "main": "run.js",
  "run_count": 1,
  "results_regexp": "^%s: (.+)$",
  "tests": [
    {"name": "Richards"},
    {"name": "DeltaBlue"},
  ]
}

V8_NESTED_SUITES_JSON = {
  "path": ["."],
  "flags": ["--flag"],
  "run_count": 1,
  "units": "score",
  "tests": [
    {"name": "Richards",
     "path": ["richards"],
     "binary": "d7",
     "main": "run.js",
     "resources": ["file1.js", "file2.js"],
     "run_count": 2,
     "results_regexp": "^Richards: (.+)$"},
    {"name": "Sub",
     "path": ["sub"],
     "tests": [
       {"name": "Leaf",
        "path": ["leaf"],
        "run_count_x64": 3,
        "units": "ms",
        "main": "run.js",
        "results_regexp": "^Simple: (.+) ms.$"},
     ]
    },
    {"name": "DeltaBlue",
     "path": ["delta_blue"],
     "main": "run.js",
     "flags": ["--flag2"],
     "results_regexp": "^DeltaBlue: (.+)$"},
    {"name": "ShouldntRun",
     "path": ["."],
     "archs": ["arm"],
     "main": "run.js"},
  ]
}

V8_GENERIC_JSON = {
  "path": ["."],
  "binary": "cc",
  "flags": ["--flag"],
  "generic": True,
  "run_count": 1,
  "units": "ms",
}

Output = namedtuple("Output", "stdout, stderr, timed_out")

class PerfTest(unittest.TestCase):
  @classmethod
  def setUpClass(cls):
    cls.base = path.dirname(path.dirname(path.abspath(__file__)))
    sys.path.append(cls.base)
    cls._cov = coverage.coverage(
        include=([os.path.join(cls.base, "run_perf.py")]))
    cls._cov.start()
    import run_perf
    from testrunner.local import commands
    global commands
    global run_perf

  @classmethod
  def tearDownClass(cls):
    cls._cov.stop()
    print ""
    print cls._cov.report()

  def setUp(self):
    self.maxDiff = None
    if path.exists(TEST_WORKSPACE):
      shutil.rmtree(TEST_WORKSPACE)
    os.makedirs(TEST_WORKSPACE)

  def tearDown(self):
    if path.exists(TEST_WORKSPACE):
      shutil.rmtree(TEST_WORKSPACE)

  def _WriteTestInput(self, json_content):
    self._test_input = path.join(TEST_WORKSPACE, "test.json")
    with open(self._test_input, "w") as f:
      f.write(json.dumps(json_content))

  def _MockCommand(self, *args, **kwargs):
    # Fake output for each test run.
    test_outputs = [Output(stdout=arg,
                           stderr=None,
                           timed_out=kwargs.get("timed_out", False))
                    for arg in args[1]]
    def execute(*args, **kwargs):
      return test_outputs.pop()
    commands.Execute = MagicMock(side_effect=execute)

    # Check that d8 is called from the correct cwd for each test run.
    dirs = [path.join(TEST_WORKSPACE, arg) for arg in args[0]]
    def chdir(*args, **kwargs):
      self.assertEquals(dirs.pop(), args[0])
    os.chdir = MagicMock(side_effect=chdir)

  def _CallMain(self, *args):
    self._test_output = path.join(TEST_WORKSPACE, "results.json")
    all_args=[
      "--json-test-results",
      self._test_output,
      self._test_input,
    ]
    all_args += args
    return run_perf.Main(all_args)

  def _LoadResults(self):
    with open(self._test_output) as f:
      return json.load(f)

  def _VerifyResults(self, suite, units, traces):
    self.assertEquals([
      {"units": units,
       "graphs": [suite, trace["name"]],
       "results": trace["results"],
       "stddev": trace["stddev"]} for trace in traces],
      self._LoadResults()["traces"])

  def _VerifyErrors(self, errors):
    self.assertEquals(errors, self._LoadResults()["errors"])

  def _VerifyMock(self, binary, *args, **kwargs):
    arg = [path.join(path.dirname(self.base), binary)]
    arg += args
    commands.Execute.assert_called_with(
        arg, timeout=kwargs.get("timeout", 60))

  def _VerifyMockMultiple(self, *args, **kwargs):
    expected = []
    for arg in args:
      a = [path.join(path.dirname(self.base), arg[0])]
      a += arg[1:]
      expected.append(((a,), {"timeout": kwargs.get("timeout", 60)}))
    self.assertEquals(expected, commands.Execute.call_args_list)

  def testOneRun(self):
    self._WriteTestInput(V8_JSON)
    self._MockCommand(["."], ["x\nRichards: 1.234\nDeltaBlue: 10657567\ny\n"])
    self.assertEquals(0, self._CallMain())
    self._VerifyResults("test", "score", [
      {"name": "Richards", "results": ["1.234"], "stddev": ""},
      {"name": "DeltaBlue", "results": ["10657567"], "stddev": ""},
    ])
    self._VerifyErrors([])
    self._VerifyMock(path.join("out", "x64.release", "d7"), "--flag", "run.js")

  def testTwoRuns_Units_SuiteName(self):
    test_input = dict(V8_JSON)
    test_input["run_count"] = 2
    test_input["name"] = "v8"
    test_input["units"] = "ms"
    self._WriteTestInput(test_input)
    self._MockCommand([".", "."],
                      ["Richards: 100\nDeltaBlue: 200\n",
                       "Richards: 50\nDeltaBlue: 300\n"])
    self.assertEquals(0, self._CallMain())
    self._VerifyResults("v8", "ms", [
      {"name": "Richards", "results": ["50", "100"], "stddev": ""},
      {"name": "DeltaBlue", "results": ["300", "200"], "stddev": ""},
    ])
    self._VerifyErrors([])
    self._VerifyMock(path.join("out", "x64.release", "d7"), "--flag", "run.js")

  def testTwoRuns_SubRegexp(self):
    test_input = dict(V8_JSON)
    test_input["run_count"] = 2
    del test_input["results_regexp"]
    test_input["tests"][0]["results_regexp"] = "^Richards: (.+)$"
    test_input["tests"][1]["results_regexp"] = "^DeltaBlue: (.+)$"
    self._WriteTestInput(test_input)
    self._MockCommand([".", "."],
                      ["Richards: 100\nDeltaBlue: 200\n",
                       "Richards: 50\nDeltaBlue: 300\n"])
    self.assertEquals(0, self._CallMain())
    self._VerifyResults("test", "score", [
      {"name": "Richards", "results": ["50", "100"], "stddev": ""},
      {"name": "DeltaBlue", "results": ["300", "200"], "stddev": ""},
    ])
    self._VerifyErrors([])
    self._VerifyMock(path.join("out", "x64.release", "d7"), "--flag", "run.js")

  def testNestedSuite(self):
    self._WriteTestInput(V8_NESTED_SUITES_JSON)
    self._MockCommand(["delta_blue", "sub/leaf", "richards"],
                      ["DeltaBlue: 200\n",
                       "Simple: 1 ms.\n",
                       "Simple: 2 ms.\n",
                       "Simple: 3 ms.\n",
                       "Richards: 100\n",
                       "Richards: 50\n"])
    self.assertEquals(0, self._CallMain())
    self.assertEquals([
      {"units": "score",
       "graphs": ["test", "Richards"],
       "results": ["50", "100"],
       "stddev": ""},
      {"units": "ms",
       "graphs": ["test", "Sub", "Leaf"],
       "results": ["3", "2", "1"],
       "stddev": ""},
      {"units": "score",
       "graphs": ["test", "DeltaBlue"],
       "results": ["200"],
       "stddev": ""},
      ], self._LoadResults()["traces"])
    self._VerifyErrors([])
    self._VerifyMockMultiple(
        (path.join("out", "x64.release", "d7"), "--flag", "file1.js",
         "file2.js", "run.js"),
        (path.join("out", "x64.release", "d7"), "--flag", "file1.js",
         "file2.js", "run.js"),
        (path.join("out", "x64.release", "d8"), "--flag", "run.js"),
        (path.join("out", "x64.release", "d8"), "--flag", "run.js"),
        (path.join("out", "x64.release", "d8"), "--flag", "run.js"),
        (path.join("out", "x64.release", "d8"), "--flag", "--flag2", "run.js"))

  def testOneRunStdDevRegExp(self):
    test_input = dict(V8_JSON)
    test_input["stddev_regexp"] = "^%s\-stddev: (.+)$"
    self._WriteTestInput(test_input)
    self._MockCommand(["."], ["Richards: 1.234\nRichards-stddev: 0.23\n"
                              "DeltaBlue: 10657567\nDeltaBlue-stddev: 106\n"])
    self.assertEquals(0, self._CallMain())
    self._VerifyResults("test", "score", [
      {"name": "Richards", "results": ["1.234"], "stddev": "0.23"},
      {"name": "DeltaBlue", "results": ["10657567"], "stddev": "106"},
    ])
    self._VerifyErrors([])
    self._VerifyMock(path.join("out", "x64.release", "d7"), "--flag", "run.js")

  def testTwoRunsStdDevRegExp(self):
    test_input = dict(V8_JSON)
    test_input["stddev_regexp"] = "^%s\-stddev: (.+)$"
    test_input["run_count"] = 2
    self._WriteTestInput(test_input)
    self._MockCommand(["."], ["Richards: 3\nRichards-stddev: 0.7\n"
                              "DeltaBlue: 6\nDeltaBlue-boom: 0.9\n",
                              "Richards: 2\nRichards-stddev: 0.5\n"
                              "DeltaBlue: 5\nDeltaBlue-stddev: 0.8\n"])
    self.assertEquals(1, self._CallMain())
    self._VerifyResults("test", "score", [
      {"name": "Richards", "results": ["2", "3"], "stddev": "0.7"},
      {"name": "DeltaBlue", "results": ["5", "6"], "stddev": "0.8"},
    ])
    self._VerifyErrors(
        ["Test Richards should only run once since a stddev is provided "
         "by the test.",
         "Test DeltaBlue should only run once since a stddev is provided "
         "by the test.",
         "Regexp \"^DeltaBlue\-stddev: (.+)$\" didn't match for test "
         "DeltaBlue."])
    self._VerifyMock(path.join("out", "x64.release", "d7"), "--flag", "run.js")

  def testBuildbot(self):
    self._WriteTestInput(V8_JSON)
    self._MockCommand(["."], ["Richards: 1.234\nDeltaBlue: 10657567\n"])
    self.assertEquals(0, self._CallMain("--buildbot"))
    self._VerifyResults("test", "score", [
      {"name": "Richards", "results": ["1.234"], "stddev": ""},
      {"name": "DeltaBlue", "results": ["10657567"], "stddev": ""},
    ])
    self._VerifyErrors([])
    self._VerifyMock(path.join("out", "Release", "d7"), "--flag", "run.js")

  def testBuildbotWithTotal(self):
    test_input = dict(V8_JSON)
    test_input["total"] = True
    self._WriteTestInput(test_input)
    self._MockCommand(["."], ["Richards: 1.234\nDeltaBlue: 10657567\n"])
    self.assertEquals(0, self._CallMain("--buildbot"))
    self._VerifyResults("test", "score", [
      {"name": "Richards", "results": ["1.234"], "stddev": ""},
      {"name": "DeltaBlue", "results": ["10657567"], "stddev": ""},
      {"name": "Total", "results": ["3626.49109719"], "stddev": ""},
    ])
    self._VerifyErrors([])
    self._VerifyMock(path.join("out", "Release", "d7"), "--flag", "run.js")

  def testBuildbotWithTotalAndErrors(self):
    test_input = dict(V8_JSON)
    test_input["total"] = True
    self._WriteTestInput(test_input)
    self._MockCommand(["."], ["x\nRichaards: 1.234\nDeltaBlue: 10657567\ny\n"])
    self.assertEquals(1, self._CallMain("--buildbot"))
    self._VerifyResults("test", "score", [
      {"name": "Richards", "results": [], "stddev": ""},
      {"name": "DeltaBlue", "results": ["10657567"], "stddev": ""},
    ])
    self._VerifyErrors(
        ["Regexp \"^Richards: (.+)$\" didn't match for test Richards.",
         "Not all traces have the same number of results."])
    self._VerifyMock(path.join("out", "Release", "d7"), "--flag", "run.js")

  def testRegexpNoMatch(self):
    self._WriteTestInput(V8_JSON)
    self._MockCommand(["."], ["x\nRichaards: 1.234\nDeltaBlue: 10657567\ny\n"])
    self.assertEquals(1, self._CallMain())
    self._VerifyResults("test", "score", [
      {"name": "Richards", "results": [], "stddev": ""},
      {"name": "DeltaBlue", "results": ["10657567"], "stddev": ""},
    ])
    self._VerifyErrors(
        ["Regexp \"^Richards: (.+)$\" didn't match for test Richards."])
    self._VerifyMock(path.join("out", "x64.release", "d7"), "--flag", "run.js")

  def testOneRunGeneric(self):
    test_input = dict(V8_GENERIC_JSON)
    self._WriteTestInput(test_input)
    self._MockCommand(["."], [
      "Trace(Test1), Result(1.234), StdDev(0.23)\n"
      "Trace(Test2), Result(10657567), StdDev(106)\n"])
    self.assertEquals(0, self._CallMain())
    self._VerifyResults("test", "ms", [
      {"name": "Test1", "results": ["1.234"], "stddev": "0.23"},
      {"name": "Test2", "results": ["10657567"], "stddev": "106"},
    ])
    self._VerifyErrors([])
    self._VerifyMock(path.join("out", "x64.release", "cc"), "--flag", "")

  def testOneRunTimingOut(self):
    test_input = dict(V8_JSON)
    test_input["timeout"] = 70
    self._WriteTestInput(test_input)
    self._MockCommand(["."], [""], timed_out=True)
    self.assertEquals(1, self._CallMain())
    self._VerifyResults("test", "score", [
      {"name": "Richards", "results": [], "stddev": ""},
      {"name": "DeltaBlue", "results": [], "stddev": ""},
    ])
    self._VerifyErrors([
      "Regexp \"^Richards: (.+)$\" didn't match for test Richards.",
      "Regexp \"^DeltaBlue: (.+)$\" didn't match for test DeltaBlue.",
    ])
    self._VerifyMock(
        path.join("out", "x64.release", "d7"), "--flag", "run.js", timeout=70)
