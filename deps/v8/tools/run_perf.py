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
import subprocess
import sys

from testrunner.local import commands
from testrunner.local import utils

ARCH_GUESS = utils.DefaultArch()
SUPPORTED_ARCHS = ["arm",
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
TOOLS_BASE = os.path.abspath(os.path.dirname(__file__))


def LoadAndroidBuildTools(path):  # pragma: no cover
  assert os.path.exists(path)
  sys.path.insert(0, path)

  from pylib.device import adb_wrapper  # pylint: disable=F0401
  from pylib.device import device_errors  # pylint: disable=F0401
  from pylib.device import device_utils  # pylint: disable=F0401
  from pylib.perf import cache_control  # pylint: disable=F0401
  from pylib.perf import perf_control  # pylint: disable=F0401
  global adb_wrapper
  global cache_control
  global device_errors
  global device_utils
  global perf_control


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


class Measurement(object):
  """Represents a series of results of one trace.

  The results are from repetitive runs of the same executable. They are
  gathered by repeated calls to ConsumeOutput.
  """
  def __init__(self, graphs, units, results_regexp, stddev_regexp):
    self.name = graphs[-1]
    self.graphs = graphs
    self.units = units
    self.results_regexp = results_regexp
    self.stddev_regexp = stddev_regexp
    self.results = []
    self.errors = []
    self.stddev = ""

  def ConsumeOutput(self, stdout):
    try:
      result = re.search(self.results_regexp, stdout, re.M).group(1)
      self.results.append(str(float(result)))
    except ValueError:
      self.errors.append("Regexp \"%s\" returned a non-numeric for test %s."
                         % (self.results_regexp, self.name))
    except:
      self.errors.append("Regexp \"%s\" didn't match for test %s."
                         % (self.results_regexp, self.name))

    try:
      if self.stddev_regexp and self.stddev:
        self.errors.append("Test %s should only run once since a stddev "
                           "is provided by the test." % self.name)
      if self.stddev_regexp:
        self.stddev = re.search(self.stddev_regexp, stdout, re.M).group(1)
    except:
      self.errors.append("Regexp \"%s\" didn't match for test %s."
                         % (self.stddev_regexp, self.name))

  def GetResults(self):
    return Results([{
      "graphs": self.graphs,
      "units": self.units,
      "results": self.results,
      "stddev": self.stddev,
    }], self.errors)


class NullMeasurement(object):
  """Null object to avoid having extra logic for configurations that didn't
  run like running without patch on trybots.
  """
  def ConsumeOutput(self, stdout):
    pass

  def GetResults(self):
    return Results()


def Unzip(iterable):
  left = []
  right = []
  for l, r in iterable:
    left.append(l)
    right.append(r)
  return lambda: iter(left), lambda: iter(right)


def AccumulateResults(
    graph_names, trace_configs, iter_output, trybot, no_patch, calc_total):
  """Iterates over the output of multiple benchmark reruns and accumulates
  results for a configured list of traces.

  Args:
    graph_names: List of names that configure the base path of the traces. E.g.
                 ['v8', 'Octane'].
    trace_configs: List of "TraceConfig" instances. Each trace config defines
                   how to perform a measurement.
    iter_output: Iterator over the standard output of each test run.
    trybot: Indicates that this is run in trybot mode, i.e. run twice, once
            with once without patch.
    no_patch: Indicates weather this is a trybot run without patch.
    calc_total: Boolean flag to speficy the calculation of a summary trace.
  Returns: A "Results" object.
  """
  measurements = [
    trace.CreateMeasurement(trybot, no_patch) for trace in trace_configs]
  for stdout in iter_output():
    for measurement in measurements:
      measurement.ConsumeOutput(stdout)

  res = reduce(lambda r, m: r + m.GetResults(), measurements, Results())

  if not res.traces or not calc_total:
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
    "graphs": graph_names + ["Total"],
    "units": res.traces[0]["units"],
    "results": total_results,
    "stddev": "",
  })
  return res


def AccumulateGenericResults(graph_names, suite_units, iter_output):
  """Iterates over the output of multiple benchmark reruns and accumulates
  generic results.

  Args:
    graph_names: List of names that configure the base path of the traces. E.g.
                 ['v8', 'Octane'].
    suite_units: Measurement default units as defined by the benchmark suite.
    iter_output: Iterator over the standard output of each test run.
  Returns: A "Results" object.
  """
  traces = OrderedDict()
  for stdout in iter_output():
    if stdout is None:
      # The None value is used as a null object to simplify logic.
      continue
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
                    "/".join(graph_names + [graph, trace])]

        trace_result = traces.setdefault(trace, Results([{
          "graphs": graph_names + [graph, trace],
          "units": (units or suite_units).strip(),
          "results": [],
          "stddev": "",
        }], errors))
        trace_result.traces[0]["results"].extend(results)
        trace_result.traces[0]["stddev"] = stddev

  return reduce(lambda r, t: r + t, traces.itervalues(), Results())


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


class GraphConfig(Node):
  """Represents a suite definition.

  Can either be a leaf or an inner node that provides default values.
  """
  def __init__(self, suite, parent, arch):
    super(GraphConfig, self).__init__()
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


class TraceConfig(GraphConfig):
  """Represents a leaf in the suite tree structure."""
  def __init__(self, suite, parent, arch):
    super(TraceConfig, self).__init__(suite, parent, arch)
    assert self.results_regexp

  def CreateMeasurement(self, trybot, no_patch):
    if not trybot and no_patch:
      # Use null object for no-patch logic if this is not a trybot run.
      return NullMeasurement()

    return Measurement(
        self.graphs,
        self.units,
        self.results_regexp,
        self.stddev_regexp,
    )


class RunnableConfig(GraphConfig):
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

  def GetCommandFlags(self, extra_flags=None):
    suffix = ["--"] + self.test_flags if self.test_flags else []
    return self.flags + (extra_flags or []) + [self.main] + suffix

  def GetCommand(self, shell_dir, extra_flags=None):
    # TODO(machenbach): This requires +.exe if run on windows.
    extra_flags = extra_flags or []
    cmd = [os.path.join(shell_dir, self.binary)]
    if self.binary != 'd8' and '--prof' in extra_flags:
      print "Profiler supported only on a benchmark run with d8"
    return cmd + self.GetCommandFlags(extra_flags=extra_flags)

  def Run(self, runner, trybot):
    """Iterates over several runs and handles the output for all traces."""
    stdout_with_patch, stdout_no_patch = Unzip(runner())
    return (
        AccumulateResults(
            self.graphs,
            self._children,
            iter_output=stdout_with_patch,
            trybot=trybot,
            no_patch=False,
            calc_total=self.total,
        ),
        AccumulateResults(
            self.graphs,
            self._children,
            iter_output=stdout_no_patch,
            trybot=trybot,
            no_patch=True,
            calc_total=self.total,
        ),
    )


class RunnableTraceConfig(TraceConfig, RunnableConfig):
  """Represents a runnable suite definition that is a leaf."""
  def __init__(self, suite, parent, arch):
    super(RunnableTraceConfig, self).__init__(suite, parent, arch)

  def Run(self, runner, trybot):
    """Iterates over several runs and handles the output."""
    measurement_with_patch = self.CreateMeasurement(trybot, False)
    measurement_no_patch = self.CreateMeasurement(trybot, True)
    for stdout_with_patch, stdout_no_patch in runner():
      measurement_with_patch.ConsumeOutput(stdout_with_patch)
      measurement_no_patch.ConsumeOutput(stdout_no_patch)
    return (
        measurement_with_patch.GetResults(),
        measurement_no_patch.GetResults(),
    )


class RunnableGenericConfig(RunnableConfig):
  """Represents a runnable suite definition with generic traces."""
  def __init__(self, suite, parent, arch):
    super(RunnableGenericConfig, self).__init__(suite, parent, arch)

  def Run(self, runner, trybot):
    stdout_with_patch, stdout_no_patch = Unzip(runner())
    return (
        AccumulateGenericResults(self.graphs, self.units, stdout_with_patch),
        AccumulateGenericResults(self.graphs, self.units, stdout_no_patch),
    )


def MakeGraphConfig(suite, arch, parent):
  """Factory method for making graph configuration objects."""
  if isinstance(parent, RunnableConfig):
    # Below a runnable can only be traces.
    return TraceConfig(suite, parent, arch)
  elif suite.get("main") is not None:
    # A main file makes this graph runnable. Empty strings are accepted.
    if suite.get("tests"):
      # This graph has subgraphs (traces).
      return RunnableConfig(suite, parent, arch)
    else:
      # This graph has no subgraphs, it's a leaf.
      return RunnableTraceConfig(suite, parent, arch)
  elif suite.get("generic"):
    # This is a generic suite definition. It is either a runnable executable
    # or has a main js file.
    return RunnableGenericConfig(suite, parent, arch)
  elif suite.get("tests"):
    # This is neither a leaf nor a runnable.
    return GraphConfig(suite, parent, arch)
  else:  # pragma: no cover
    raise Exception("Invalid suite configuration.")


def BuildGraphConfigs(suite, arch, parent=None):
  """Builds a tree structure of graph objects that corresponds to the suite
  configuration.
  """
  parent = parent or DefaultSentinel()

  # TODO(machenbach): Implement notion of cpu type?
  if arch not in suite.get("archs", SUPPORTED_ARCHS):
    return None

  graph = MakeGraphConfig(suite, arch, parent)
  for subsuite in suite.get("tests", []):
    BuildGraphConfigs(subsuite, arch, graph)
  parent.AppendChild(graph)
  return graph


def FlattenRunnables(node, node_cb):
  """Generator that traverses the tree structure and iterates over all
  runnables.
  """
  node_cb(node)
  if isinstance(node, RunnableConfig):
    yield node
  elif isinstance(node, Node):
    for child in node._children:
      for result in FlattenRunnables(child, node_cb):
        yield result
  else:  # pragma: no cover
    raise Exception("Invalid suite configuration.")


class Platform(object):
  def __init__(self, options):
    self.shell_dir = options.shell_dir
    self.shell_dir_no_patch = options.shell_dir_no_patch
    self.extra_flags = options.extra_flags.split()

  @staticmethod
  def GetPlatform(options):
    if options.android_build_tools:
      return AndroidPlatform(options)
    else:
      return DesktopPlatform(options)

  def _Run(self, runnable, count, no_patch=False):
    raise NotImplementedError()  # pragma: no cover

  def Run(self, runnable, count):
    """Execute the benchmark's main file.

    If options.shell_dir_no_patch is specified, the benchmark is run once with
    and once without patch.
    Args:
      runnable: A Runnable benchmark instance.
      count: The number of this (repeated) run.
    Returns: A tuple with the benchmark outputs with and without patch. The
             latter will be None if options.shell_dir_no_patch was not
             specified.
    """
    stdout = self._Run(runnable, count, no_patch=False)
    if self.shell_dir_no_patch:
      return stdout, self._Run(runnable, count, no_patch=True)
    else:
      return stdout, None


class DesktopPlatform(Platform):
  def __init__(self, options):
    super(DesktopPlatform, self).__init__(options)

  def PreExecution(self):
    pass

  def PostExecution(self):
    pass

  def PreTests(self, node, path):
    if isinstance(node, RunnableConfig):
      node.ChangeCWD(path)

  def _Run(self, runnable, count, no_patch=False):
    suffix = ' - without patch' if no_patch else ''
    shell_dir = self.shell_dir_no_patch if no_patch else self.shell_dir
    title = ">>> %%s (#%d)%s:" % ((count + 1), suffix)
    try:
      output = commands.Execute(
          runnable.GetCommand(shell_dir, self.extra_flags),
          timeout=runnable.timeout,
      )
    except OSError as e:  # pragma: no cover
      print title % "OSError"
      print e
      return ""
    print title % "Stdout"
    print output.stdout
    if output.stderr:  # pragma: no cover
      # Print stderr for debugging.
      print title % "Stderr"
      print output.stderr
    if output.timed_out:
      print ">>> Test timed out after %ss." % runnable.timeout
    if '--prof' in self.extra_flags:
      os_prefix = {"linux": "linux", "macos": "mac"}.get(utils.GuessOS())
      if os_prefix:
        tick_tools = os.path.join(TOOLS_BASE, "%s-tick-processor" % os_prefix)
        subprocess.check_call(tick_tools + " --only-summary", shell=True)
      else:  # pragma: no cover
        print "Profiler option currently supported on Linux and Mac OS."
    return output.stdout


class AndroidPlatform(Platform):  # pragma: no cover
  DEVICE_DIR = "/data/local/tmp/v8/"

  def __init__(self, options):
    super(AndroidPlatform, self).__init__(options)
    LoadAndroidBuildTools(options.android_build_tools)

    if not options.device:
      # Detect attached device if not specified.
      devices = adb_wrapper.AdbWrapper.Devices()
      assert devices and len(devices) == 1, (
          "None or multiple devices detected. Please specify the device on "
          "the command-line with --device")
      options.device = str(devices[0])
    self.adb_wrapper = adb_wrapper.AdbWrapper(options.device)
    self.device = device_utils.DeviceUtils(self.adb_wrapper)

  def PreExecution(self):
    perf = perf_control.PerfControl(self.device)
    perf.SetHighPerfMode()

    # Remember what we have already pushed to the device.
    self.pushed = set()

  def PostExecution(self):
    perf = perf_control.PerfControl(self.device)
    perf.SetDefaultPerfMode()
    self.device.RunShellCommand(["rm", "-rf", AndroidPlatform.DEVICE_DIR])

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
    output = self.adb_wrapper.Push(file_on_host, file_on_device_tmp)
    # Success looks like this: "3035 KB/s (12512056 bytes in 4.025s)".
    # Errors look like this: "failed to copy  ... ".
    if output and not re.search('^[0-9]', output.splitlines()[-1]):
      logging.critical('PUSH FAILED: ' + output)
    self.adb_wrapper.Shell("mkdir -p %s" % folder_on_device)
    self.adb_wrapper.Shell("cp %s %s" % (file_on_device_tmp, file_on_device))

  def _PushExecutable(self, shell_dir, target_dir, binary):
    self._PushFile(shell_dir, binary, target_dir)

    # Push external startup data. Backwards compatible for revisions where
    # these files didn't exist.
    self._PushFile(
        shell_dir,
        "natives_blob.bin",
        target_dir,
        skip_if_missing=True,
    )
    self._PushFile(
        shell_dir,
        "snapshot_blob.bin",
        target_dir,
        skip_if_missing=True,
    )

  def PreTests(self, node, path):
    suite_dir = os.path.abspath(os.path.dirname(path))
    if node.path:
      bench_rel = os.path.normpath(os.path.join(*node.path))
      bench_abs = os.path.join(suite_dir, bench_rel)
    else:
      bench_rel = "."
      bench_abs = suite_dir

    self._PushExecutable(self.shell_dir, "bin", node.binary)
    if self.shell_dir_no_patch:
      self._PushExecutable(
          self.shell_dir_no_patch, "bin_no_patch", node.binary)

    if isinstance(node, RunnableConfig):
      self._PushFile(bench_abs, node.main, bench_rel)
    for resource in node.resources:
      self._PushFile(bench_abs, resource, bench_rel)

  def _Run(self, runnable, count, no_patch=False):
    suffix = ' - without patch' if no_patch else ''
    target_dir = "bin_no_patch" if no_patch else "bin"
    title = ">>> %%s (#%d)%s:" % ((count + 1), suffix)
    cache = cache_control.CacheControl(self.device)
    cache.DropRamCaches()
    binary_on_device = os.path.join(
        AndroidPlatform.DEVICE_DIR, target_dir, runnable.binary)
    cmd = [binary_on_device] + runnable.GetCommandFlags(self.extra_flags)

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
      print title % "Stdout"
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
                    help="Path to chromium's build/android. Specifying this "
                         "option will run tests using android platform.")
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
  parser.add_option("--extra-flags",
                    help="Additional flags to pass to the test executable",
                    default="")
  parser.add_option("--json-test-results",
                    help="Path to a file for storing json results.")
  parser.add_option("--json-test-results-no-patch",
                    help="Path to a file for storing json results from run "
                         "without patch.")
  parser.add_option("--outdir", help="Base directory with compile output",
                    default="out")
  parser.add_option("--outdir-no-patch",
                    help="Base directory with compile output without patch")
  (options, args) = parser.parse_args(args)

  if len(args) == 0:  # pragma: no cover
    parser.print_help()
    return 1

  if options.arch in ["auto", "native"]:  # pragma: no cover
    options.arch = ARCH_GUESS

  if not options.arch in SUPPORTED_ARCHS:  # pragma: no cover
    print "Unknown architecture %s" % options.arch
    return 1

  if options.device and not options.android_build_tools:  # pragma: no cover
    print "Specifying a device requires Android build tools."
    return 1

  if (options.json_test_results_no_patch and
      not options.outdir_no_patch):  # pragma: no cover
    print("For writing json test results without patch, an outdir without "
          "patch must be specified.")
    return 1

  workspace = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))

  if options.buildbot:
    build_config = "Release"
  else:
    build_config = "%s.release" % options.arch

  options.shell_dir = os.path.join(workspace, options.outdir, build_config)

  if options.outdir_no_patch:
    options.shell_dir_no_patch = os.path.join(
        workspace, options.outdir_no_patch, build_config)
  else:
    options.shell_dir_no_patch = None

  platform = Platform.GetPlatform(options)

  results = Results()
  results_no_patch = Results()
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
    root = BuildGraphConfigs(suite, options.arch)

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
      result, result_no_patch = runnable.Run(
          Runner, trybot=options.shell_dir_no_patch)
      results += result
      results_no_patch += result_no_patch
    platform.PostExecution()

  if options.json_test_results:
    results.WriteToFile(options.json_test_results)
  else:  # pragma: no cover
    print results

  if options.json_test_results_no_patch:
    results_no_patch.WriteToFile(options.json_test_results_no_patch)
  else:  # pragma: no cover
    print results_no_patch

  return min(1, len(results.errors))

if __name__ == "__main__":  # pragma: no cover
  sys.exit(Main(sys.argv[1:]))
