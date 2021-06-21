#!/usr/bin/env python
# Copyright 2020 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This is main driver for gcmole tool. See README for more details.
# Usage: CLANG_BIN=clang-bin-dir python tools/gcmole/gcmole.py [arm|arm64|ia32|x64]

# for py2/py3 compatibility
from __future__ import print_function

import collections
import difflib
from multiprocessing import cpu_count
import os
import re
import subprocess
import sys
import threading
if sys.version_info.major > 2:
  import queue
else:
  import Queue as queue

ArchCfg = collections.namedtuple("ArchCfg",
                                 ["triple", "arch_define", "arch_options"])

ARCHITECTURES = {
    "ia32":
        ArchCfg(
            triple="i586-unknown-linux",
            arch_define="V8_TARGET_ARCH_IA32",
            arch_options=["-m32"],
        ),
    "arm":
        ArchCfg(
            triple="i586-unknown-linux",
            arch_define="V8_TARGET_ARCH_ARM",
            arch_options=["-m32"],
        ),
    "x64":
        ArchCfg(
            triple="x86_64-unknown-linux",
            arch_define="V8_TARGET_ARCH_X64",
            arch_options=[]),
    "arm64":
        ArchCfg(
            triple="x86_64-unknown-linux",
            arch_define="V8_TARGET_ARCH_ARM64",
            arch_options=[],
        ),
}


def log(format, *args):
  print(format.format(*args))


def fatal(format, *args):
  log(format, *args)
  sys.exit(1)


# -----------------------------------------------------------------------------
# Clang invocation


def MakeClangCommandLine(plugin, plugin_args, arch_cfg, clang_bin_dir,
                         clang_plugins_dir):
  prefixed_plugin_args = []
  if plugin_args:
    for arg in plugin_args:
      prefixed_plugin_args += [
          "-Xclang",
          "-plugin-arg-{}".format(plugin),
          "-Xclang",
          arg,
      ]

  return ([
      os.path.join(clang_bin_dir, "clang++"),
      "-std=c++14",
      "-c",
      "-Xclang",
      "-load",
      "-Xclang",
      os.path.join(clang_plugins_dir, "libgcmole.so"),
      "-Xclang",
      "-plugin",
      "-Xclang",
      plugin,
  ] + prefixed_plugin_args + [
      "-Xclang",
      "-triple",
      "-Xclang",
      arch_cfg.triple,
      "-fno-exceptions",
      "-D",
      arch_cfg.arch_define,
      "-DENABLE_DEBUGGER_SUPPORT",
      "-DV8_INTL_SUPPORT",
      "-DV8_ENABLE_WEBASSEMBLY",
      "-I./",
      "-Iinclude/",
      "-Iout/build/gen",
      "-Ithird_party/icu/source/common",
      "-Ithird_party/icu/source/i18n",
  ] + arch_cfg.arch_options)


def InvokeClangPluginForFile(filename, cmd_line, verbose):
  if verbose:
    print("popen ", " ".join(cmd_line + [filename]))
  p = subprocess.Popen(
      cmd_line + [filename], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
  stdout, stderr = p.communicate()
  return p.returncode, stdout, stderr


def InvokeClangPluginForFilesInQueue(i, input_queue, output_queue, cancel_event,
                                     cmd_line, verbose):
  success = False
  try:
    while not cancel_event.is_set():
      filename = input_queue.get_nowait()
      ret, stdout, stderr = InvokeClangPluginForFile(filename, cmd_line,
                                                     verbose)
      output_queue.put_nowait((filename, ret, stdout.decode('utf-8'), stderr.decode('utf-8')))
      if ret != 0:
        break
  except KeyboardInterrupt:
    log("-- [{}] Interrupting", i)
  except queue.Empty:
    success = True
  finally:
    # Emit a success bool so that the reader knows that there was either an
    # error or all files were processed.
    output_queue.put_nowait(success)


def InvokeClangPluginForEachFile(
    filenames,
    plugin,
    plugin_args,
    arch_cfg,
    flags,
    clang_bin_dir,
    clang_plugins_dir,
):
  cmd_line = MakeClangCommandLine(plugin, plugin_args, arch_cfg, clang_bin_dir,
                                  clang_plugins_dir)
  verbose = flags["verbose"]
  if flags["sequential"]:
    log("** Sequential execution.")
    for filename in filenames:
      log("-- {}", filename)
      returncode, stdout, stderr = InvokeClangPluginForFile(
          filename, cmd_line, verbose)
      if returncode != 0:
        sys.stderr.write(stderr)
        sys.exit(returncode)
      yield filename, stdout, stderr
  else:
    log("** Parallel execution.")
    cpus = cpu_count()
    input_queue = queue.Queue()
    output_queue = queue.Queue()
    threads = []
    try:
      for filename in filenames:
        input_queue.put(filename)

      cancel_event = threading.Event()

      for i in range(min(len(filenames), cpus)):
        threads.append(
            threading.Thread(
                target=InvokeClangPluginForFilesInQueue,
                args=(i, input_queue, output_queue, cancel_event, cmd_line,
                      verbose)))

      for t in threads:
        t.start()

      num_finished = 0
      while num_finished < len(threads):
        output = output_queue.get()
        if type(output) == bool:
          if output:
            num_finished += 1
            continue
          else:
            break
        filename, returncode, stdout, stderr = output
        log("-- {}", filename)
        if returncode != 0:
          sys.stderr.write(stderr)
          sys.exit(returncode)
        yield filename, stdout, stderr

    finally:
      cancel_event.set()
      for t in threads:
        t.join()


# -----------------------------------------------------------------------------


def ParseGNFile(for_test):
  result = {}
  if for_test:
    gn_files = [("tools/gcmole/GCMOLE.gn", re.compile('"([^"]*?\.cc)"'), "")]
  else:
    gn_files = [
        ("BUILD.gn", re.compile('"([^"]*?\.cc)"'), ""),
        ("test/cctest/BUILD.gn", re.compile('"(test-[^"]*?\.cc)"'),
         "test/cctest/"),
    ]

  for filename, pattern, prefix in gn_files:
    with open(filename) as gn_file:
      gn = gn_file.read()
      for condition, sources in re.findall("### gcmole\((.*?)\) ###(.*?)\]", gn,
                                           re.MULTILINE | re.DOTALL):
        if condition not in result:
          result[condition] = []
        for file in pattern.findall(sources):
          result[condition].append(prefix + file)

  return result


def EvaluateCondition(cond, props):
  if cond == "all":
    return True

  m = re.match("(\w+):(\w+)", cond)
  if m is None:
    fatal("failed to parse condition: {}", cond)
  p, v = m.groups()
  if p not in props:
    fatal("undefined configuration property: {}", p)

  return props[p] == v


def BuildFileList(sources, props):
  ret = []
  for condition, files in sources.items():
    if EvaluateCondition(condition, props):
      ret += files
  return ret


gn_sources = ParseGNFile(for_test=False)
gn_test_sources = ParseGNFile(for_test=True)


def FilesForArch(arch):
  return BuildFileList(gn_sources, {
      "os": "linux",
      "arch": arch,
      "mode": "debug",
      "simulator": ""
  })


def FilesForTest(arch):
  return BuildFileList(gn_test_sources, {
      "os": "linux",
      "arch": arch,
      "mode": "debug",
      "simulator": ""
  })


# -----------------------------------------------------------------------------
# GCSuspects Generation

# Note that the gcsuspects file lists functions in the form:
#  mangled_name,unmangled_function_name
#
# This means that we can match just the function name by matching only
# after a comma.
ALLOWLIST = [
    # The following functions call CEntryStub which is always present.
    "MacroAssembler.*,CallRuntime",
    "CompileCallLoadPropertyWithInterceptor",
    "CallIC.*,GenerateMiss",
    # DirectCEntryStub is a special stub used on ARM.
    # It is pinned and always present.
    "DirectCEntryStub.*,GenerateCall",
    # TODO GCMole currently is sensitive enough to understand that certain
    #    functions only cause GC and return Failure simulataneously.
    #    Callsites of such functions are safe as long as they are properly
    #    check return value and propagate the Failure to the caller.
    #    It should be possible to extend GCMole to understand this.
    "Heap.*,TryEvacuateObject",
    # Ignore all StateTag methods.
    "StateTag",
    # Ignore printing of elements transition.
    "PrintElementsTransition",
    # CodeCreateEvent receives AbstractCode (a raw ptr) as an argument.
    "CodeCreateEvent",
    "WriteField",
]

GC_PATTERN = ",.*Collect.*Garbage"
SAFEPOINT_PATTERN = ",SafepointSlowPath"
ALLOWLIST_PATTERN = "|".join("(?:%s)" % p for p in ALLOWLIST)


def MergeRegexp(pattern_dict):
  return re.compile("|".join(
      "(?P<%s>%s)" % (key, value) for (key, value) in pattern_dict.items()))


IS_SPECIAL_WITHOUT_ALLOW_LIST = MergeRegexp({
    "gc": GC_PATTERN,
    "safepoint": SAFEPOINT_PATTERN
})
IS_SPECIAL_WITH_ALLOW_LIST = MergeRegexp({
    "gc": GC_PATTERN,
    "safepoint": SAFEPOINT_PATTERN,
    "allow": ALLOWLIST_PATTERN
})


class GCSuspectsCollector:

  def __init__(self, flags):
    self.gc = {}
    self.gc_caused = collections.defaultdict(lambda: [])
    self.funcs = {}
    self.current_caller = None
    self.allowlist = flags["allowlist"]
    self.is_special = IS_SPECIAL_WITH_ALLOW_LIST if self.allowlist else IS_SPECIAL_WITHOUT_ALLOW_LIST

  def AddCause(self, name, cause):
    self.gc_caused[name].append(cause)

  def Parse(self, lines):
    for funcname in lines:
      if not funcname:
        continue

      if funcname[0] != "\t":
        self.Resolve(funcname)
        self.current_caller = funcname
      else:
        name = funcname[1:]
        callers_for_name = self.Resolve(name)
        callers_for_name.add(self.current_caller)

  def Resolve(self, name):
    if name not in self.funcs:
      self.funcs[name] = set()
      m = self.is_special.search(name)
      if m:
        if m.group("gc"):
          self.gc[name] = True
          self.AddCause(name, "<GC>")
        elif m.group("safepoint"):
          self.gc[name] = True
          self.AddCause(name, "<Safepoint>")
        elif m.group("allow"):
          self.gc[name] = False

    return self.funcs[name]

  def Propagate(self):
    log("** Propagating GC information")

    def mark(funcname, callers):
      for caller in callers:
        if caller not in self.gc:
          self.gc[caller] = True
          mark(caller, self.funcs[caller])

        self.AddCause(caller, funcname)

    for funcname, callers in self.funcs.items():
      if self.gc.get(funcname, False):
        mark(funcname, callers)


def GenerateGCSuspects(arch, files, arch_cfg, flags, clang_bin_dir,
                       clang_plugins_dir):
  # Reset the global state.
  collector = GCSuspectsCollector(flags)

  log("** Building GC Suspects for {}", arch)
  for filename, stdout, stderr in InvokeClangPluginForEachFile(
      files, "dump-callees", [], arch_cfg, flags, clang_bin_dir,
      clang_plugins_dir):
    collector.Parse(stdout.splitlines())

  collector.Propagate()

  with open("gcsuspects", "w") as out:
    for name, value in collector.gc.items():
      if value:
        out.write(name + "\n")

  with open("gccauses", "w") as out:
    out.write("GC = {\n")
    for name, causes in collector.gc_caused.items():
      out.write("  '{}': [\n".format(name))
      for cause in causes:
        out.write("    '{}',\n".format(cause))
      out.write("  ],\n")
    out.write("}\n")

  log("** GCSuspects generated for {}", arch)


# ------------------------------------------------------------------------------
# Analysis


def CheckCorrectnessForArch(arch, for_test, flags, clang_bin_dir,
                            clang_plugins_dir):
  if for_test:
    files = FilesForTest(arch)
  else:
    files = FilesForArch(arch)
  arch_cfg = ARCHITECTURES[arch]

  if not flags["reuse_gcsuspects"]:
    GenerateGCSuspects(arch, files, arch_cfg, flags, clang_bin_dir,
                       clang_plugins_dir)
  else:
    log("** Reusing GCSuspects for {}", arch)

  processed_files = 0
  errors_found = False
  output = ""

  log(
      "** Searching for evaluation order problems{} for {}",
      " and dead variables" if flags["dead_vars"] else "",
      arch,
  )
  plugin_args = []
  if flags["dead_vars"]:
    plugin_args.append("--dead-vars")
  if flags["verbose_trace"]:
    plugin_args.append("--verbose")
  for filename, stdout, stderr in InvokeClangPluginForEachFile(
      files,
      "find-problems",
      plugin_args,
      arch_cfg,
      flags,
      clang_bin_dir,
      clang_plugins_dir,
  ):
    processed_files = processed_files + 1
    if not errors_found:
      errors_found = re.search("^[^:]+:\d+:\d+: (warning|error)", stderr,
                               re.MULTILINE) is not None
    if for_test:
      output = output + stderr
    else:
      sys.stdout.write(stderr)

  log(
      "** Done processing {} files. {}",
      processed_files,
      "Errors found" if errors_found else "No errors found",
  )

  return errors_found, output


def TestRun(flags, clang_bin_dir, clang_plugins_dir):
  log("** Test Run")
  errors_found, output = CheckCorrectnessForArch("x64", True, flags,
                                                 clang_bin_dir,
                                                 clang_plugins_dir)
  if not errors_found:
    log("** Test file should produce errors, but none were found. Output:")
    log(output)
    return False

  filename = "tools/gcmole/test-expectations.txt"
  with open(filename) as exp_file:
    expectations = exp_file.read()

  if output != expectations:
    log("** Output mismatch from running tests. Please run them manually.")

    for line in difflib.unified_diff(
        expectations.splitlines(),
        output.splitlines(),
        fromfile=filename,
        tofile="output",
        lineterm="",
    ):
      log("{}", line)

    log("------")
    log("--- Full output ---")
    log(output)
    log("------")

    return False

  log("** Tests ran successfully")
  return True


def main(args):
  DIR = os.path.dirname(args[0])

  clang_bin_dir = os.getenv("CLANG_BIN")
  clang_plugins_dir = os.getenv("CLANG_PLUGINS")

  if not clang_bin_dir or clang_bin_dir == "":
    fatal("CLANG_BIN not set")

  if not clang_plugins_dir or clang_plugins_dir == "":
    clang_plugins_dir = DIR

  flags = {
      #: not build gcsuspects file and reuse previously generated one.
      "reuse_gcsuspects": False,
      #:n't use parallel python runner.
      "sequential": False,
      # Print commands to console before executing them.
      "verbose": True,
      # Perform dead variable analysis.
      "dead_vars": True,
      # Enable verbose tracing from the plugin itself.
      "verbose_trace": False,
      # When building gcsuspects allowlist certain functions as if they can be
      # causing GC. Currently used to reduce number of false positives in dead
      # variables analysis. See TODO for ALLOWLIST
      "allowlist": True,
  }
  pos_args = []

  flag_regexp = re.compile("^--(no[-_]?)?([\w\-_]+)$")
  for arg in args[1:]:
    m = flag_regexp.match(arg)
    if m:
      no, flag = m.groups()
      flag = flag.replace("-", "_")
      if flag in flags:
        flags[flag] = no is None
      else:
        fatal("Unknown flag: {}", flag)
    else:
      pos_args.append(arg)

  archs = pos_args if len(pos_args) > 0 else ["ia32", "arm", "x64", "arm64"]

  any_errors_found = False
  if not TestRun(flags, clang_bin_dir, clang_plugins_dir):
    any_errors_found = True
  else:
    for arch in archs:
      if not ARCHITECTURES[arch]:
        fatal("Unknown arch: {}", arch)

      errors_found, output = CheckCorrectnessForArch(arch, False, flags,
                                                     clang_bin_dir,
                                                     clang_plugins_dir)
      any_errors_found = any_errors_found or errors_found

  sys.exit(1 if any_errors_found else 0)


if __name__ == "__main__":
  main(sys.argv)
