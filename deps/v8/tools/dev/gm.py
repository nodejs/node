#!/usr/bin/env python2
# Copyright 2017 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""\
Convenience wrapper for compiling V8 with gn/ninja and running tests.
Sets up build output directories if they don't exist.
Produces simulator builds for non-Intel target architectures.
Uses Goma by default if it is detected (at output directory setup time).
Expects to be run from the root of a V8 checkout.

Usage:
    gm.py [<arch>].[<mode>[-<suffix>]].[<target>] [testname...] [--flag]

All arguments are optional. Most combinations should work, e.g.:
    gm.py ia32.debug x64.release x64.release-my-custom-opts d8
    gm.py android_arm.release.check --progress=verbose
    gm.py x64 mjsunit/foo cctest/test-bar/*

Flags are passed unchanged to the test runner. They must start with -- and must
not contain spaces.
"""
# See HELP below for additional documentation.
# Note on Python3 compatibility: gm.py itself is Python3 compatible, but
# run-tests.py, which will be executed by the same binary, is not; hence
# the hashbang line at the top of this file explicitly requires Python2.

from __future__ import print_function
import errno
import os
import re
import subprocess
import sys

USE_PTY = "linux" in sys.platform
if USE_PTY:
  import pty

BUILD_TARGETS_TEST = ["d8", "cctest", "inspector-test", "unittests",
                      "wasm_api_tests"]
BUILD_TARGETS_ALL = ["all"]

# All arches that this script understands.
ARCHES = ["ia32", "x64", "arm", "arm64", "mipsel", "mips64el", "ppc", "ppc64",
          "riscv64", "s390", "s390x", "android_arm", "android_arm64"]
# Arches that get built/run when you don't specify any.
DEFAULT_ARCHES = ["ia32", "x64", "arm", "arm64"]
# Modes that this script understands.
MODES = ["release", "debug", "optdebug"]
# Modes that get built/run when you don't specify any.
DEFAULT_MODES = ["release", "debug"]
# Build targets that can be manually specified.
TARGETS = ["d8", "cctest", "unittests", "v8_fuzzers", "wasm_api_tests", "wee8",
           "mkgrokdump", "generate-bytecode-expectations", "inspector-test"]
# Build targets that get built when you don't specify any (and specified tests
# don't imply any other targets).
DEFAULT_TARGETS = ["d8"]
# Tests that run-tests.py would run by default that can be run with
# BUILD_TARGETS_TESTS.
DEFAULT_TESTS = ["cctest", "debugger", "intl", "message", "mjsunit",
                 "unittests"]
# These can be suffixed to any <arch>.<mode> combo, or used standalone,
# or used as global modifiers (affecting all <arch>.<mode> combos).
ACTIONS = {
  "all": {"targets": BUILD_TARGETS_ALL, "tests": []},
  "tests": {"targets": BUILD_TARGETS_TEST, "tests": []},
  "check": {"targets": BUILD_TARGETS_TEST, "tests": DEFAULT_TESTS},
  "checkall": {"targets": BUILD_TARGETS_ALL, "tests": ["ALL"]},
}

HELP = """<arch> can be any of: %(arches)s
<mode> can be any of: %(modes)s
<target> can be any of:
 - %(targets)s (build respective binary)
 - all (build all binaries)
 - tests (build test binaries)
 - check (build test binaries, run most tests)
 - checkall (build all binaries, run more tests)
""" % {"arches": " ".join(ARCHES),
       "modes": " ".join(MODES),
       "targets": ", ".join(TARGETS)}

TESTSUITES_TARGETS = {"benchmarks": "d8",
              "cctest": "cctest",
              "debugger": "d8",
              "fuzzer": "v8_fuzzers",
              "inspector": "inspector-test",
              "intl": "d8",
              "message": "d8",
              "mjsunit": "d8",
              "mozilla": "d8",
              "test262": "d8",
              "unittests": "unittests",
              "wasm-api-tests": "wasm_api_tests",
              "wasm-js": "d8",
              "wasm-spec-tests": "d8",
              "webkit": "d8"}

OUTDIR = "out"

def _Which(cmd):
  for path in os.environ["PATH"].split(os.pathsep):
    if os.path.exists(os.path.join(path, cmd)):
      return os.path.join(path, cmd)
  return None

def DetectGoma():
  if os.environ.get("GOMA_DIR"):
    return os.environ.get("GOMA_DIR")
  if os.environ.get("GOMADIR"):
    return os.environ.get("GOMADIR")
  # There is a copy of goma in depot_tools, but it might not be in use on
  # this machine.
  goma = _Which("goma_ctl")
  if goma is None: return None
  cipd_bin = os.path.join(os.path.dirname(goma), ".cipd_bin")
  if not os.path.exists(cipd_bin): return None
  goma_auth = os.path.expanduser("~/.goma_client_oauth2_config")
  if not os.path.exists(goma_auth): return None
  return cipd_bin

GOMADIR = DetectGoma()
IS_GOMA_MACHINE = GOMADIR is not None

USE_GOMA = "true" if IS_GOMA_MACHINE else "false"

RELEASE_ARGS_TEMPLATE = """\
is_component_build = false
is_debug = false
%s
use_goma = {GOMA}
v8_enable_backtrace = true
v8_enable_disassembler = true
v8_enable_object_print = true
v8_enable_verify_heap = true
""".replace("{GOMA}", USE_GOMA)

DEBUG_ARGS_TEMPLATE = """\
is_component_build = true
is_debug = true
symbol_level = 2
%s
use_goma = {GOMA}
v8_enable_backtrace = true
v8_enable_fast_mksnapshot = true
v8_enable_slow_dchecks = true
v8_optimized_debug = false
""".replace("{GOMA}", USE_GOMA)

OPTDEBUG_ARGS_TEMPLATE = """\
is_component_build = true
is_debug = true
symbol_level = 1
%s
use_goma = {GOMA}
v8_enable_backtrace = true
v8_enable_fast_mksnapshot = true
v8_enable_verify_heap = true
v8_optimized_debug = true
""".replace("{GOMA}", USE_GOMA)

ARGS_TEMPLATES = {
  "release": RELEASE_ARGS_TEMPLATE,
  "debug": DEBUG_ARGS_TEMPLATE,
  "optdebug": OPTDEBUG_ARGS_TEMPLATE
}

def PrintHelpAndExit():
  print(__doc__)
  print(HELP)
  sys.exit(0)

def PrintCompletionsAndExit():
  for a in ARCHES:
    print("%s" % a)
    for m in MODES:
      print("%s" % m)
      print("%s.%s" % (a, m))
      for t in TARGETS:
        print("%s" % t)
        print("%s.%s.%s" % (a, m, t))
  sys.exit(0)

def _Call(cmd, silent=False):
  if not silent: print("# %s" % cmd)
  return subprocess.call(cmd, shell=True)

def _CallWithOutputNoTerminal(cmd):
  return subprocess.check_output(cmd, stderr=subprocess.STDOUT, shell=True)

def _CallWithOutput(cmd):
  print("# %s" % cmd)
  # The following trickery is required so that the 'cmd' thinks it's running
  # in a real terminal, while this script gets to intercept its output.
  parent, child = pty.openpty()
  p = subprocess.Popen(cmd, shell=True, stdin=child, stdout=child, stderr=child)
  os.close(child)
  output = []
  try:
    while True:
      try:
        data = os.read(parent, 512).decode('utf-8')
      except OSError as e:
        if e.errno != errno.EIO: raise
        break # EIO means EOF on some systems
      else:
        if not data: # EOF
          break
        print(data, end="")
        sys.stdout.flush()
        output.append(data)
  finally:
    os.close(parent)
    p.wait()
  return p.returncode, "".join(output)

def _Write(filename, content):
  print("# echo > %s << EOF\n%sEOF" % (filename, content))
  with open(filename, "w") as f:
    f.write(content)

def _Notify(summary, body):
  if _Which('notify-send') is not None:
    _Call("notify-send '{}' '{}'".format(summary, body), silent=True)
  else:
    print("{} - {}".format(summary, body))

def _GetMachine():
  # Once we migrate to Python3, this can use os.uname().machine.
  # The index-based access is compatible with all Python versions.
  return os.uname()[4]

def GetPath(arch, mode):
  subdir = "%s.%s" % (arch, mode)
  return os.path.join(OUTDIR, subdir)

def PrepareMksnapshotCmdline(orig_cmdline, path):
  result = "gdb --args %s/mksnapshot " % path
  for w in orig_cmdline.split(" "):
    if w.startswith("gen/") or w.startswith("snapshot_blob"):
      result += ("%(path)s%(sep)s%(arg)s " %
                 {"path": path, "sep": os.sep, "arg": w})
    else:
      result += "%s " % w
  return result

class Config(object):
  def __init__(self, arch, mode, targets, tests=[], testrunner_args=[]):
    self.arch = arch
    self.mode = mode
    self.targets = set(targets)
    self.tests = set(tests)
    self.testrunner_args = testrunner_args

  def Extend(self, targets, tests=[]):
    self.targets.update(targets)
    self.tests.update(tests)

  def GetTargetCpu(self):
    cpu = "x86"
    if self.arch == "android_arm":
      cpu = "arm"
    elif self.arch == "android_arm64":
      cpu = "arm64"
    elif self.arch == "arm64" and _GetMachine() == "aarch64":
      # arm64 build host:
      cpu = "arm64"
    elif self.arch == "arm" and _GetMachine() == "aarch64":
      cpu = "arm"
    elif "64" in self.arch or self.arch == "s390x":
      # Native x64 or simulator build.
      cpu = "x64"
    return ["target_cpu = \"%s\"" % cpu]

  def GetV8TargetCpu(self):
    if self.arch == "android_arm":
      v8_cpu = "arm"
    elif self.arch == "android_arm64":
      v8_cpu = "arm64"
    elif self.arch in ("arm", "arm64", "mipsel", "mips64el", "ppc", "ppc64",
                       "riscv64", "s390", "s390x"):
      v8_cpu = self.arch
    else:
      return []
    return ["v8_target_cpu = \"%s\"" % v8_cpu]

  def GetTargetOS(self):
    if self.arch in ("android_arm", "android_arm64"):
      return ["target_os = \"android\""]
    return []

  def GetSpecialCompiler(self):
    if _GetMachine() == "aarch64":
      # We have no prebuilt Clang for arm64. Use the system Clang instead.
      return ["clang_base_path = \"/usr\"", "clang_use_chrome_plugins = false"]
    return []

  def GetGnArgs(self):
    # Use only substring before first '-' as the actual mode
    mode = re.match("([^-]+)", self.mode).group(1)
    template = ARGS_TEMPLATES[mode]
    arch_specific = (self.GetTargetCpu() + self.GetV8TargetCpu() +
                     self.GetTargetOS() + self.GetSpecialCompiler())
    return template % "\n".join(arch_specific)

  def Build(self):
    path = GetPath(self.arch, self.mode)
    args_gn = os.path.join(path, "args.gn")
    build_ninja = os.path.join(path, "build.ninja")
    if not os.path.exists(path):
      print("# mkdir -p %s" % path)
      os.makedirs(path)
    if not os.path.exists(args_gn):
      _Write(args_gn, self.GetGnArgs())
    if not os.path.exists(build_ninja):
      code = _Call("gn gen %s" % path)
      if code != 0: return code
    targets = " ".join(self.targets)
    # The implementation of mksnapshot failure detection relies on
    # the "pty" module and GDB presence, so skip it on non-Linux.
    if not USE_PTY:
      return _Call("autoninja -C %s %s" % (path, targets))

    return_code, output = _CallWithOutput("autoninja -C %s %s" %
                                          (path, targets))
    if return_code != 0 and "FAILED:" in output and "snapshot_blob" in output:
      csa_trap = re.compile("Specify option( --csa-trap-on-node=[^ ]*)")
      match = csa_trap.search(output)
      extra_opt = match.group(1) if match else ""
      cmdline = re.compile("python ../../tools/run.py ./mksnapshot (.*)")
      orig_cmdline = cmdline.search(output).group(1).strip()
      cmdline = PrepareMksnapshotCmdline(orig_cmdline, path) + extra_opt
      _Notify("V8 build requires your attention",
              "Detected mksnapshot failure, re-running in GDB...")
      _Call(cmdline)
    return return_code

  def RunTests(self):
    # Special handling for "mkgrokdump": if it was built, run it.
    if (self.arch == "x64" and self.mode == "release" and
        "mkgrokdump" in self.targets):
      _Call("%s/mkgrokdump > tools/v8heapconst.py" %
            GetPath(self.arch, self.mode))
    if not self.tests: return 0
    if "ALL" in self.tests:
      tests = ""
    else:
      tests = " ".join(self.tests)
    return _Call('"%s" ' % sys.executable +
                 os.path.join("tools", "run-tests.py") +
                 " --outdir=%s %s %s" % (
                     GetPath(self.arch, self.mode), tests,
                     " ".join(self.testrunner_args)))

def GetTestBinary(argstring):
  for suite in TESTSUITES_TARGETS:
    if argstring.startswith(suite): return TESTSUITES_TARGETS[suite]
  return None

class ArgumentParser(object):
  def __init__(self):
    self.global_targets = set()
    self.global_tests = set()
    self.global_actions = set()
    self.configs = {}
    self.testrunner_args = []

  def PopulateConfigs(self, arches, modes, targets, tests):
    for a in arches:
      for m in modes:
        path = GetPath(a, m)
        if path not in self.configs:
          self.configs[path] = Config(a, m, targets, tests,
                  self.testrunner_args)
        else:
          self.configs[path].Extend(targets, tests)

  def ProcessGlobalActions(self):
    have_configs = len(self.configs) > 0
    for action in self.global_actions:
      impact = ACTIONS[action]
      if (have_configs):
        for c in self.configs:
          self.configs[c].Extend(**impact)
      else:
        self.PopulateConfigs(DEFAULT_ARCHES, DEFAULT_MODES, **impact)

  def ParseArg(self, argstring):
    if argstring in ("-h", "--help", "help"):
      PrintHelpAndExit()
    if argstring == "--print-completions":
      PrintCompletionsAndExit()
    arches = []
    modes = []
    targets = []
    actions = []
    tests = []
    # Special handling for "mkgrokdump": build it for x64.release.
    if argstring == "mkgrokdump":
      self.PopulateConfigs(["x64"], ["release"], ["mkgrokdump"], [])
      return
    # Specifying a single unit test looks like "unittests/Foo.Bar", test262
    # tests have names like "S15.4.4.7_A4_T1", don't split these.
    if argstring.startswith("unittests/") or argstring.startswith("test262/"):
      words = [argstring]
    elif argstring.startswith("--"):
      # Pass all other flags to test runner.
      self.testrunner_args.append(argstring)
      return
    else:
      # Assume it's a word like "x64.release" -> split at the dot.
      words = argstring.split('.')
    if len(words) == 1:
      word = words[0]
      if word in ACTIONS:
        self.global_actions.add(word)
        return
      if word in TARGETS:
        self.global_targets.add(word)
        return
      maybe_target = GetTestBinary(word)
      if maybe_target is not None:
        self.global_tests.add(word)
        self.global_targets.add(maybe_target)
        return
    for word in words:
      if word in ARCHES:
        arches.append(word)
      elif word in MODES:
        modes.append(word)
      elif word in TARGETS:
        targets.append(word)
      elif word in ACTIONS:
        actions.append(word)
      elif any(map(lambda x: word.startswith(x + "-"), MODES)):
        modes.append(word)
      else:
        print("Didn't understand: %s" % word)
        sys.exit(1)
    # Process actions.
    for action in actions:
      impact = ACTIONS[action]
      targets += impact["targets"]
      tests += impact["tests"]
    # Fill in defaults for things that weren't specified.
    arches = arches or DEFAULT_ARCHES
    modes = modes or DEFAULT_MODES
    targets = targets or DEFAULT_TARGETS
    # Produce configs.
    self.PopulateConfigs(arches, modes, targets, tests)

  def ParseArguments(self, argv):
    if len(argv) == 0:
      PrintHelpAndExit()
    for argstring in argv:
      self.ParseArg(argstring)
    self.ProcessGlobalActions()
    for c in self.configs:
      self.configs[c].Extend(self.global_targets, self.global_tests)
    return self.configs

def Main(argv):
  parser = ArgumentParser()
  configs = parser.ParseArguments(argv[1:])
  return_code = 0
  # If we have Goma but it is not running, start it.
  if (IS_GOMA_MACHINE and
      _Call("ps -e | grep compiler_proxy > /dev/null", silent=True) != 0):
    _Call("%s/goma_ctl.py ensure_start" % GOMADIR)
  for c in configs:
    return_code += configs[c].Build()
  if return_code == 0:
    for c in configs:
      return_code += configs[c].RunTests()
  if return_code == 0:
    _Notify('Done!', 'V8 compilation finished successfully.')
  else:
    _Notify('Error!', 'V8 compilation finished with errors.')
  return return_code

if __name__ == "__main__":
  sys.exit(Main(sys.argv))
