#!/usr/bin/env python
# Copyright 2014 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Performance runner for d8.

Call e.g. with tools/run-perf.py --arch ia32 some_suite.json

The suite json format is expected to be:
{
  "path": <relative path chunks to perf resources and main file>,
  "name": <optional suite name, file name is default>,
  "archs": [<architecture name for which this suite is run>, ...],
  "binary": <name of binary to run, default "d8">,
  "flags": [<flag to d8>, ...],
  "test_flags": [<flag to the test file>, ...],
  "run_count": <how often will this suite run (optional)>,
  "run_count_XXX": <how often will this suite run for arch XXX (optional)>,
  "resources": [<js file to be moved to android device>, ...]
  "main": <main js perf runner file>,
  "results_regexp": <optional regexp>,
  "results_processor": <optional python results processor script>,
  "units": <the unit specification for the performance dashboard>,
  "tests": [
    {
      "name": <name of the trace>,
      "results_regexp": <optional more specific regexp>,
      "results_processor": <optional python results processor script>,
      "units": <the unit specification for the performance dashboard>,
    }, ...
  ]
}

The tests field can also nest other suites in arbitrary depth. A suite
with a "main" file is a leaf suite that can contain one more level of
tests.

A suite's results_regexp is expected to have one string place holder
"%s" for the trace name. A trace's results_regexp overwrites suite
defaults.

A suite's results_processor may point to an optional python script. If
specified, it is called after running the tests like this (with a path
relatve to the suite level's path):
<results_processor file> <same flags as for d8> <suite level name> <output>

The <output> is a temporary file containing d8 output. The results_regexp will
be applied to the output of this script.

A suite without "tests" is considered a performance test itself.

Full example (suite with one runner):
{
  "path": ["."],
  "flags": ["--expose-gc"],
  "test_flags": ["5"],
  "archs": ["ia32", "x64"],
  "run_count": 5,
  "run_count_ia32": 3,
  "main": "run.js",
  "results_regexp": "^%s: (.+)$",
  "units": "score",
  "tests": [
    {"name": "Richards"},
    {"name": "DeltaBlue"},
    {"name": "NavierStokes",
     "results_regexp": "^NavierStokes: (.+)$"}
  ]
}

Full example (suite with several runners):
{
  "path": ["."],
  "flags": ["--expose-gc"],
  "archs": ["ia32", "x64"],
  "run_count": 5,
  "units": "score",
  "tests": [
    {"name": "Richards",
     "path": ["richards"],
     "main": "run.js",
     "run_count": 3,
     "results_regexp": "^Richards: (.+)$"},
    {"name": "NavierStokes",
     "path": ["navier_stokes"],
     "main": "run.js",
     "results_regexp": "^NavierStokes: (.+)$"}
  ]
}

Path pieces are concatenated. D8 is always run with the suite's path as cwd.

The test flags are passed to the js test file after '--'.
"""

from collections import OrderedDict
import json
import logging
import math
import optparse
import os
import re
import sys

from testrunner.local import commands
from testrunner.local import utils

ARCH_GUESS = utils.DefaultArch()
SUPPORTED_ARCHS = ["android_arm",
                   "android_arm64",
                   "android_ia32",
                   "android_x64",
                   "arm",
                   "ia32",
                   "mips",
                   "mipsel",
                   "nacl_ia32",
                   "nacl_x64",
                   "x64",
                   "arm64"]

GENERIC_RESULTS_RE = re.compile(r"^RESULT ([^:]+): ([^=]+)= ([^ ]+) ([^ ]*)$")
RESULT_STDDEV_RE = re.compile(r"^\{([^\}]+)\}$")
RESULT_LIST_RE = re.compile(r"^\[([^\]]+)\]$")


def LoadAndroidBuildTools(path):  # pragma: no cover
  assert os.path.exists(path)
  sys.path.insert(0, path)

  from pylib.device import device_utils  # pylint: disable=F0401
  from pylib.device import device_errors  # pylint: disable=F0401
  from pylib.perf import cache_control  # pylint: disable=F0401
  from pylib.perf import perf_control  # pylint: disable=F0401
  import pylib.android_commands  # pylint: disable=F0401
  global cache_control
  global device_errors
  global device_utils
  global perf_control
  global pylib


def GeometricMean(values):
  """Returns the geometric mean of a list of values.

  The mean is calculated using log to avoid overflow.
  """
  values = map(float, values)
  return str(math.exp(sum(map(math.log, values)) / len(values)))


class Results(object):
  """Place holder for result traces."""
  def __init__(self, traces=None, errors=None):
    self.traces = traces or []
    self.errors = errors or []

  def ToDict(self):
    return {"traces": self.traces, "errors": self.errors}

  def WriteToFile(self, file_name):
    with open(file_name, "w") as f:
      f.write(json.dumps(self.ToDict()))

  def __add__(self, other):
    self.traces += other.traces
    self.errors += other.errors
    return self

  def __str__(self):  # pragma: no cover
    return str(self.ToDict())


class Node(object):
  """Represents a node in the suite tree structure."""
  def __init__(self, *args):
    self._children = []

  def AppendChild(self, child):
    self._children.append(child)


class DefaultSentinel(Node):
  """Fake parent node with all default values."""
  def __init__(self):
    super(DefaultSentinel, self).__init__()
    self.binary = "d8"
    self.run_count = 10
    self.timeout = 60
    self.path = []
    self.graphs = []
    self.flags = []
    self.test_flags = []
    self.resources = []
    self.results_regexp = None
    self.stddev_regexp = None
    self.units = "score"
    self.total = False


class Graph(Node):
  """Represents a suite definition.

  Can either be a leaf or an inner node that provides default values.
  """
  def __init__(self, suite, parent, arch):
    super(Graph, self).__init__()
    self._suite = suite

    assert isinstance(suite.get("path", []), list)
    assert isinstance(suite["name"], basestring)
    assert isinstance(suite.get("flags", []), list)
    assert isinstance(suite.get("test_flags", []), list)
    assert isinstance(suite.get("resources", []), list)

    # Accumulated values.
    self.path = parent.path[:] + suite.get("path", [])
    self.graphs = parent.graphs[:] + [suite["name"]]
    self.flags = parent.flags[:] + suite.get("flags", [])
    self.test_flags = parent.test_flags[:] + suite.get("test_flags", [])

    # Values independent of parent node.
    self.resources = suite.get("resources", [])

    # Descrete values (with parent defaults).
    self.binary = suite.get("binary", parent.binary)
    self.run_count = suite.get("run_count", parent.run_count)
    self.run_count = suite.get("run_count_%s" % arch, self.run_count)
    self.timeout = suite.get("timeout", parent.timeout)
    self.timeout = suite.get("timeout_%s" % arch, self.timeout)
    self.units = suite.get("units", parent.units)
    self.total = suite.get("total", parent.total)

    # A regular expression for results. If the parent graph provides a
    # regexp and the current suite has none, a string place holder for the
    # suite name is expected.
    # TODO(machenbach): Currently that makes only sense for the leaf level.
    # Multiple place holders for multiple levels are not supported.
    if parent.results_regexp:
      regexp_default = parent.results_regexp % re.escape(suite["name"])
    else:
      regexp_default = None
    self.results_regexp = suite.get("results_regexp", regexp_default)

    # A similar regular expression for the standard deviation (optional).
    if parent.stddev_regexp:
      stddev_default = parent.stddev_regexp % re.escape(suite["name"])
    else:
      stddev_default = None
    self.stddev_regexp = suite.get("stddev_regexp", stddev_default)


class Trace(Graph):
  """Represents a leaf in the suite tree structure.

  Handles collection of measurements.
  """
  def __init__(self, suite, parent, arch):
    super(Trace, self).__init__(suite, parent, arch)
    assert self.results_regexp
    self.results = []
    self.errors = []
    self.stddev = ""

  def ConsumeOutput(self, stdout):
    try:
      result = re.search(self.results_regexp, stdout, re.M).group(1)
      self.results.append(str(float(result)))
    except ValueError:
      self.errors.append("Regexp \"%s\" returned a non-numeric for test %s."
                         % (self.results_regexp, self.graphs[-1]))
    except:
      self.errors.append("Regexp \"%s\" didn't match for test %s."
                         % (self.results_regexp, self.graphs[-1]))

    try:
      if self.stddev_regexp and self.stddev:
        self.errors.append("Test %s should only run once since a stddev "
                           "is provided by the test." % self.graphs[-1])
      if self.stddev_regexp:
        self.stddev = re.search(self.stddev_regexp, stdout, re.M).group(1)
    except:
      self.errors.append("Regexp \"%s\" didn't match for test %s."
                         % (self.stddev_regexp, self.graphs[-1]))

  def GetResults(self):
    return Results([{
      "graphs": self.graphs,
      "units": self.units,
      "results": self.results,
      "stddev": self.stddev,
    }], self.errors)


class Runnable(Graph):
  """Represents a runnable suite definition (i.e. has a main file).
  """
  @property
  def main(self):
    return self._suite.get("main", "")

  def ChangeCWD(self, suite_path):
    """Changes the cwd to to path defined in the current graph.

    The tests are supposed to be relative to the suite configuration.
    """
    suite_dir = os.path.abspath(os.path.dirname(suite_path))
    bench_dir = os.path.normpath(os.path.join(*self.path))
    os.chdir(os.path.join(suite_dir, bench_dir))

  def GetCommandFlags(self):
    suffix = ["--"] + self.test_flags if self.test_flags else []
    return self.flags + [self.main] + suffix

  def GetCommand(self, shell_dir):
    # TODO(machenbach): This requires +.exe if run on windows.
    return [os.path.join(shell_dir, self.binary)] + self.GetCommandFlags()

  def Run(self, runner):
    """Iterates over several runs and handles the output for all traces."""
    for stdout in runner():
      for trace in self._children:
        trace.ConsumeOutput(stdout)
    res = reduce(lambda r, t: r + t.GetResults(), self._children, Results())

    if not res.traces or not self.total:
      return res

    # Assume all traces have the same structure.
    if len(set(map(lambda t: len(t["results"]), res.traces))) != 1:
      res.errors.append("Not all traces have the same number of results.")
      return res

    # Calculate the geometric means for all traces. Above we made sure that
    # there is at least one trace and that the number of results is the same
    # for each trace.
    n_results = len(res.traces[0]["results"])
    total_results = [GeometricMean(t["results"][i] for t in res.traces)
                     for i in range(0, n_results)]
    res.traces.append({
      "graphs": self.graphs + ["Total"],
      "units": res.traces[0]["units"],
      "results": total_results,
      "stddev": "",
    })
    return res

class RunnableTrace(Trace, Runnable):
  """Represents a runnable suite definition that is a leaf."""
  def __init__(self, suite, parent, arch):
    super(RunnableTrace, self).__init__(suite, parent, arch)

  def Run(self, runner):
    """Iterates over several runs and handles the output."""
    for stdout in runner():
      self.ConsumeOutput(stdout)
    return self.GetResults()


class RunnableGeneric(Runnable):
  """Represents a runnable suite definition with generic traces."""
  def __init__(self, suite, parent, arch):
    super(RunnableGeneric, self).__init__(suite, parent, arch)

  def Run(self, runner):
    """Iterates over several runs and handles the output."""
    traces = OrderedDict()
    for stdout in runner():
      for line in stdout.strip().splitlines():
        match = GENERIC_RESULTS_RE.match(line)
        if match:
          stddev = ""
          graph = match.group(1)
          trace = match.group(2)
          body = match.group(3)
          units = match.group(4)
          match_stddev = RESULT_STDDEV_RE.match(body)
          match_list = RESULT_LIST_RE.match(body)
          errors = []
          if match_stddev:
            result, stddev = map(str.strip, match_stddev.group(1).split(","))
            results = [result]
          elif match_list:
            results = map(str.strip, match_list.group(1).split(","))
          else:
            results = [body.strip()]

          try:
            results = map(lambda r: str(float(r)), results)
          except ValueError:
            results = []
            errors = ["Found non-numeric in %s" %
                      "/".join(self.graphs + [graph, trace])]

          trace_result = traces.setdefault(trace, Results([{
            "graphs": self.graphs + [graph, trace],
            "units": (units or self.units).strip(),
            "results": [],
            "stddev": "",
          }], errors))
          trace_result.traces[0]["results"].extend(results)
          trace_result.traces[0]["stddev"] = stddev

    return reduce(lambda r, t: r + t, traces.itervalues(), Results())


def MakeGraph(suite, arch, parent):
  """Factory method for making graph objects."""
  if isinstance(parent, Runnable):
    # Below a runnable can only be traces.
    return Trace(suite, parent, arch)
  elif suite.get("main"):
    # A main file makes this graph runnable.
    if suite.get("tests"):
      # This graph has subgraphs (traces).
      return Runnable(suite, parent, arch)
    else:
      # This graph has no subgraphs, it's a leaf.
      return RunnableTrace(suite, parent, arch)
  elif suite.get("generic"):
    # This is a generic suite definition. It is either a runnable executable
    # or has a main js file.
    return RunnableGeneric(suite, parent, arch)
  elif suite.get("tests"):
    # This is neither a leaf nor a runnable.
    return Graph(suite, parent, arch)
  else:  # pragma: no cover
    raise Exception("Invalid suite configuration.")


def BuildGraphs(suite, arch, parent=None):
  """Builds a tree structure of graph objects that corresponds to the suite
  configuration.
  """
  parent = parent or DefaultSentinel()

  # TODO(machenbach): Implement notion of cpu type?
  if arch not in suite.get("archs", SUPPORTED_ARCHS):
    return None

  graph = MakeGraph(suite, arch, parent)
  for subsuite in suite.get("tests", []):
    BuildGraphs(subsuite, arch, graph)
  parent.AppendChild(graph)
  return graph


def FlattenRunnables(node, node_cb):
  """Generator that traverses the tree structure and iterates over all
  runnables.
  """
  node_cb(node)
  if isinstance(node, Runnable):
    yield node
  elif isinstance(node, Node):
    for child in node._children:
      for result in FlattenRunnables(child, node_cb):
        yield result
  else:  # pragma: no cover
    raise Exception("Invalid suite configuration.")


class Platform(object):
  @staticmethod
  def GetPlatform(options):
    if options.arch.startswith("android"):
      return AndroidPlatform(options)
    else:
      return DesktopPlatform(options)


class DesktopPlatform(Platform):
  def __init__(self, options):
    self.shell_dir = options.shell_dir

  def PreExecution(self):
    pass

  def PostExecution(self):
    pass

  def PreTests(self, node, path):
    if isinstance(node, Runnable):
      node.ChangeCWD(path)

  def Run(self, runnable, count):
    output = commands.Execute(runnable.GetCommand(self.shell_dir),
                              timeout=runnable.timeout)
    print ">>> Stdout (#%d):" % (count + 1)
    print output.stdout
    if output.stderr:  # pragma: no cover
      # Print stderr for debugging.
      print ">>> Stderr (#%d):" % (count + 1)
      print output.stderr
    if output.timed_out:
      print ">>> Test timed out after %ss." % runnable.timeout
    return output.stdout


class AndroidPlatform(Platform):  # pragma: no cover
  DEVICE_DIR = "/data/local/tmp/v8/"

  def __init__(self, options):
    self.shell_dir = options.shell_dir
    LoadAndroidBuildTools(options.android_build_tools)

    if not options.device:
      # Detect attached device if not specified.
      devices = pylib.android_commands.GetAttachedDevices(
          hardware=True, emulator=False, offline=False)
      assert devices and len(devices) == 1, (
          "None or multiple devices detected. Please specify the device on "
          "the command-line with --device")
      options.device = devices[0]
    adb_wrapper = pylib.android_commands.AndroidCommands(options.device)
    self.device = device_utils.DeviceUtils(adb_wrapper)
    self.adb = adb_wrapper.Adb()

  def PreExecution(self):
    perf = perf_control.PerfControl(self.device)
    perf.SetHighPerfMode()

    # Remember what we have already pushed to the device.
    self.pushed = set()

  def PostExecution(self):
    perf = perf_control.PerfControl(self.device)
    perf.SetDefaultPerfMode()
    self.device.RunShellCommand(["rm", "-rf", AndroidPlatform.DEVICE_DIR])

  def _SendCommand(self, cmd):
    logging.info("adb -s %s %s" % (str(self.device), cmd))
    return self.adb.SendCommand(cmd, timeout_time=60)

  def _PushFile(self, host_dir, file_name, target_rel=".",
                skip_if_missing=False):
    file_on_host = os.path.join(host_dir, file_name)
    file_on_device_tmp = os.path.join(
        AndroidPlatform.DEVICE_DIR, "_tmp_", file_name)
    file_on_device = os.path.join(
        AndroidPlatform.DEVICE_DIR, target_rel, file_name)
    folder_on_device = os.path.dirname(file_on_device)

    # Only attempt to push files that exist.
    if not os.path.exists(file_on_host):
      if not skip_if_missing:
        logging.critical('Missing file on host: %s' % file_on_host)
      return

    # Only push files not yet pushed in one execution.
    if file_on_host in self.pushed:
      return
    else:
      self.pushed.add(file_on_host)

    # Work-around for "text file busy" errors. Push the files to a temporary
    # location and then copy them with a shell command.
    output = self._SendCommand(
        "push %s %s" % (file_on_host, file_on_device_tmp))
    # Success looks like this: "3035 KB/s (12512056 bytes in 4.025s)".
    # Errors look like this: "failed to copy  ... ".
    if output and not re.search('^[0-9]', output.splitlines()[-1]):
      logging.critical('PUSH FAILED: ' + output)
    self._SendCommand("shell mkdir -p %s" % folder_on_device)
    self._SendCommand("shell cp %s %s" % (file_on_device_tmp, file_on_device))

  def PreTests(self, node, path):
    suite_dir = os.path.abspath(os.path.dirname(path))
    if node.path:
      bench_rel = os.path.normpath(os.path.join(*node.path))
      bench_abs = os.path.join(suite_dir, bench_rel)
    else:
      bench_rel = "."
      bench_abs = suite_dir

    self._PushFile(self.shell_dir, node.binary)

    # Push external startup data. Backwards compatible for revisions where
    # these files didn't exist.
    self._PushFile(self.shell_dir, "natives_blob.bin", skip_if_missing=True)
    self._PushFile(self.shell_dir, "snapshot_blob.bin", skip_if_missing=True)

    if isinstance(node, Runnable):
      self._PushFile(bench_abs, node.main, bench_rel)
    for resource in node.resources:
      self._PushFile(bench_abs, resource, bench_rel)

  def Run(self, runnable, count):
    cache = cache_control.CacheControl(self.device)
    cache.DropRamCaches()
    binary_on_device = AndroidPlatform.DEVICE_DIR + runnable.binary
    cmd = [binary_on_device] + runnable.GetCommandFlags()

    # Relative path to benchmark directory.
    if runnable.path:
      bench_rel = os.path.normpath(os.path.join(*runnable.path))
    else:
      bench_rel = "."

    try:
      output = self.device.RunShellCommand(
          cmd,
          cwd=os.path.join(AndroidPlatform.DEVICE_DIR, bench_rel),
          timeout=runnable.timeout,
          retries=0,
      )
      stdout = "\n".join(output)
      print ">>> Stdout (#%d):" % (count + 1)
      print stdout
    except device_errors.CommandTimeoutError:
      print ">>> Test timed out after %ss." % runnable.timeout
      stdout = ""
    return stdout


# TODO: Implement results_processor.
def Main(args):
  logging.getLogger().setLevel(logging.INFO)
  parser = optparse.OptionParser()
  parser.add_option("--android-build-tools",
                    help="Path to chromium's build/android.")
  parser.add_option("--arch",
                    help=("The architecture to run tests for, "
                          "'auto' or 'native' for auto-detect"),
                    default="x64")
  parser.add_option("--buildbot",
                    help="Adapt to path structure used on buildbots",
                    default=False, action="store_true")
  parser.add_option("--device",
                    help="The device ID to run Android tests on. If not given "
                         "it will be autodetected.")
  parser.add_option("--json-test-results",
                    help="Path to a file for storing json results.")
  parser.add_option("--outdir", help="Base directory with compile output",
                    default="out")
  (options, args) = parser.parse_args(args)

  if len(args) == 0:  # pragma: no cover
    parser.print_help()
    return 1

  if options.arch in ["auto", "native"]:  # pragma: no cover
    options.arch = ARCH_GUESS

  if not options.arch in SUPPORTED_ARCHS:  # pragma: no cover
    print "Unknown architecture %s" % options.arch
    return 1

  if (bool(options.arch.startswith("android")) !=
      bool(options.android_build_tools)):  # pragma: no cover
    print ("Android architectures imply setting --android-build-tools and the "
           "other way around.")
    return 1

  if (options.device and not
      options.arch.startswith("android")):  # pragma: no cover
    print "Specifying a device requires an Android architecture to be used."
    return 1

  workspace = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))

  if options.buildbot:
    options.shell_dir = os.path.join(workspace, options.outdir, "Release")
  else:
    options.shell_dir = os.path.join(workspace, options.outdir,
                                     "%s.release" % options.arch)

  platform = Platform.GetPlatform(options)

  results = Results()
  for path in args:
    path = os.path.abspath(path)

    if not os.path.exists(path):  # pragma: no cover
      results.errors.append("Configuration file %s does not exist." % path)
      continue

    with open(path) as f:
      suite = json.loads(f.read())

    # If no name is given, default to the file name without .json.
    suite.setdefault("name", os.path.splitext(os.path.basename(path))[0])

    # Setup things common to one test suite.
    platform.PreExecution()

    # Build the graph/trace tree structure.
    root = BuildGraphs(suite, options.arch)

    # Callback to be called on each node on traversal.
    def NodeCB(node):
      platform.PreTests(node, path)

    # Traverse graph/trace tree and interate over all runnables.
    for runnable in FlattenRunnables(root, NodeCB):
      print ">>> Running suite: %s" % "/".join(runnable.graphs)

      def Runner():
        """Output generator that reruns several times."""
        for i in xrange(0, max(1, runnable.run_count)):
          # TODO(machenbach): Allow timeout per arch like with run_count per
          # arch.
          yield platform.Run(runnable, i)

      # Let runnable iterate over all runs and handle output.
      results += runnable.Run(Runner)

    platform.PostExecution()

  if options.json_test_results:
    results.WriteToFile(options.json_test_results)
  else:  # pragma: no cover
    print results

  return min(1, len(results.errors))

if __name__ == "__main__":  # pragma: no cover
  sys.exit(Main(sys.argv[1:]))
