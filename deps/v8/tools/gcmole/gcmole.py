#!/usr/bin/env python3
# Copyright 2020 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This is main driver for gcmole tool. See README for more details.
# Usage: CLANG_BIN=clang-bin-dir python tools/gcmole/gcmole.py [arm|arm64|ia32|x64]

from multiprocessing import cpu_count
from pathlib import Path

import collections
import difflib
import json
import optparse
import os
import re
import subprocess
import sys
import threading
import queue


ArchCfg = collections.namedtuple(
    "ArchCfg", ["name", "cpu", "triple", "arch_define", "arch_options"])

# TODO(cbruni): use gn desc by default for platform-specific settings
OPTIONS_64BIT = [
    "-DV8_COMPRESS_POINTERS",
    "-DV8_COMPRESS_POINTERS_IN_SHARED_CAGE",
    "-DV8_EXTERNAL_CODE_SPACE",
    "-DV8_SHORT_BUILTIN_CALLS",
    "-DV8_SHARED_RO_HEAP",
]

ARCHITECTURES = {
    "ia32":
        ArchCfg(
            name="ia32",
            cpu="x86",
            triple="i586-unknown-linux",
            arch_define="V8_TARGET_ARCH_IA32",
            arch_options=["-m32"],
        ),
    "arm":
        ArchCfg(
            name="arm",
            cpu="arm",
            triple="i586-unknown-linux",
            arch_define="V8_TARGET_ARCH_ARM",
            arch_options=["-m32"],
        ),
    # TODO(cbruni): Use detailed settings:
    #   arch_options = OPTIONS_64BIT + [ "-DV8_WIN64_UNWINDING_INFO" ]
    "x64":
        ArchCfg(
            name="x64",
            cpu="x64",
            triple="x86_64-unknown-linux",
            arch_define="V8_TARGET_ARCH_X64",
            arch_options=[]),
    "arm64":
        ArchCfg(
            name="arm64",
            cpu="arm64",
            triple="x86_64-unknown-linux",
            arch_define="V8_TARGET_ARCH_ARM64",
            arch_options=[],
        ),
}
ARCHITECTURES['x86'] = ARCHITECTURES['ia32']


def log(format, *args, **kwargs):
  mark = ("#", "=", "-", ".")[kwargs.get("level", 0)]
  print(mark * 2, str(format).format(*list(map(str, args))))


def fatal(format):
  log(format)
  sys.exit(1)


# -----------------------------------------------------------------------------
# Clang invocation


def make_clang_command_line(plugin, plugin_args, options):
  arch_cfg = ARCHITECTURES[options.v8_target_cpu]
  prefixed_plugin_args = []
  if plugin_args:
    for arg in plugin_args:
      prefixed_plugin_args += [
          "-Xclang",
          "-plugin-arg-" + plugin,
          "-Xclang",
          arg,
      ]
  log("Using generated files in {}", options.v8_build_dir / 'gen')
  icu_src_dir = options.v8_root_dir / 'third_party/icu/source'
  return ([
      options.clang_bin_dir / "clang++",
      "-std=c++17",
      "-c",
      "-Xclang",
      "-load",
      "-Xclang",
      options.clang_plugins_dir / "libgcmole.so",
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
      "-Wno-everything",
      "-D",
      arch_cfg.arch_define,
      "-DENABLE_DEBUGGER_SUPPORT",
      "-DV8_ENABLE_WEBASSEMBLY",
      "-DV8_GC_MOLE",
      "-DV8_INTL_SUPPORT",
      "-I{}".format(options.v8_root_dir),
      "-I{}".format(options.v8_root_dir / 'include'),
      "-I{}".format(options.v8_build_dir / 'gen'),
      "-I{}".format(icu_src_dir / 'common'),
      "-I{}".format(icu_src_dir / 'i18n'),
  ] + arch_cfg.arch_options)


def invoke_clang_plugin_for_file(filename, cmd_line, verbose):
  args = cmd_line + [filename]
  args = list(map(str, args))
  if verbose:
    print("popen ", " ".join(args))
  p = subprocess.Popen(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
  stdout, stderr = p.communicate()
  return p.returncode, stdout.decode("utf-8"), stderr.decode("utf-8")


def invoke_clang_plugin_for_files_in_queue(i, input_queue, output_queue,
                                           cancel_event, cmd_line, verbose):
  success = False
  try:
    while not cancel_event.is_set():
      filename = input_queue.get_nowait()
      ret, stdout, stderr = invoke_clang_plugin_for_file(
          filename, cmd_line, verbose)
      output_queue.put_nowait((filename, ret, stdout, stderr))
      if ret != 0:
        break
  except KeyboardInterrupt:
    log("[{}] Interrupting", i, level=1)
  except queue.Empty:
    success = True
  finally:
    # Emit a success bool so that the reader knows that there was either an
    # error or all files were processed.
    output_queue.put_nowait(success)


def invoke_clang_plugin_for_each_file(filenames, plugin, plugin_args, options):
  cmd_line = make_clang_command_line(plugin, plugin_args, options)
  verbose = options.verbose
  if options.sequential:
    log("Sequential execution.")
    for filename in filenames:
      log(filename, level=1)
      returncode, stdout, stderr = invoke_clang_plugin_for_file(
          filename, cmd_line, verbose)
      if returncode != 0:
        sys.stderr.write(stderr)
        sys.exit(returncode)
      yield filename, stdout, stderr
  else:
    log("Parallel execution.")
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
                target=invoke_clang_plugin_for_files_in_queue,
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
        log(filename, level=2)
        if returncode != 0:
          sys.stderr.write(stderr)
          sys.exit(returncode)
        yield filename, stdout, stderr

    finally:
      cancel_event.set()
      for t in threads:
        t.join()


# -----------------------------------------------------------------------------


def parse_gn_file(options, for_test):
  if for_test:
    return {"all": [options.v8_root_dir / "tools/gcmole/gcmole-test.cc"]}
  result = {}
  gn_files = [
      ("BUILD.gn", re.compile('"([^"]*?\.cc)"'), ""),
      ("test/cctest/BUILD.gn", re.compile('"(test-[^"]*?\.cc)"'),
       Path("test/cctest/")),
  ]
  for filename, pattern, prefix in gn_files:
    path = options.v8_root_dir / filename
    with open(path) as gn_file:
      gn = gn_file.read()
      for condition, sources in re.findall("### gcmole\((.*?)\) ###(.*?)\]", gn,
                                           re.MULTILINE | re.DOTALL):
        if condition not in result:
          result[condition] = []
        for file in pattern.findall(sources):
          result[condition].append(options.v8_root_dir / prefix / file)

  return result


def evaluate_condition(cond, props):
  if cond == "all":
    return True

  m = re.match("(\w+):(\w+)", cond)
  if m is None:
    fatal("failed to parse condition: {}", cond)
  p, v = m.groups()
  if p not in props:
    fatal("undefined configuration property: {}", p)

  return props[p] == v


def build_file_list(options, for_test):
  sources = parse_gn_file(options, for_test)
  props = {
      "os": "linux",
      "arch": options.v8_target_cpu,
      "mode": "debug",
      "simulator": ""
  }
  ret = []
  for condition, files in list(sources.items()):
    if evaluate_condition(condition, props):
      ret += files
  return ret


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
ALLOWLIST_PATTERN = "|".join("(?:{})".format(p) for p in ALLOWLIST)


def merge_regexp(pattern_dict):
  return re.compile("|".join("(?P<{}>{})".format(key, value)
                             for (key, value) in list(pattern_dict.items())))


IS_SPECIAL_WITHOUT_ALLOW_LIST = merge_regexp({
    "gc": GC_PATTERN,
    "safepoint": SAFEPOINT_PATTERN
})
IS_SPECIAL_WITH_ALLOW_LIST = merge_regexp({
    "gc": GC_PATTERN,
    "safepoint": SAFEPOINT_PATTERN,
    "allow": ALLOWLIST_PATTERN
})


class GCSuspectsCollector:

  def __init__(self, options):
    self.gc = {}
    self.gc_caused = collections.defaultdict(lambda: set())
    self.funcs = {}
    self.current_caller = None
    self.allowlist = options.allowlist
    self.is_special = IS_SPECIAL_WITH_ALLOW_LIST if self.allowlist else IS_SPECIAL_WITHOUT_ALLOW_LIST

  def add_cause(self, name, cause):
    self.gc_caused[name].add(cause)

  def parse(self, lines):
    for funcname in lines:
      if not funcname:
        continue

      if funcname[0] != "\t":
        self.resolve(funcname)
        self.current_caller = funcname
      else:
        name = funcname[1:]
        callers_for_name = self.resolve(name)
        callers_for_name.add(self.current_caller)

  def resolve(self, name):
    if name not in self.funcs:
      self.funcs[name] = set()
      m = self.is_special.search(name)
      if m:
        if m.group("gc"):
          self.gc[name] = True
          self.add_cause(name, "<GC>")
        elif m.group("safepoint"):
          self.gc[name] = True
          self.add_cause(name, "<Safepoint>")
        elif m.group("allow"):
          self.gc[name] = False

    return self.funcs[name]

  def propagate(self):
    log("Propagating GC information")

    def mark(funcname, callers):
      for caller in callers:
        if caller not in self.gc:
          self.gc[caller] = True
          mark(caller, self.funcs[caller])
        self.add_cause(caller, funcname)

    for funcname, callers in list(self.funcs.items()):
      if self.gc.get(funcname, False):
        mark(funcname, callers)


def generate_gc_suspects(files, options):
  # Reset the global state.
  collector = GCSuspectsCollector(options)

  log("Building GC Suspects for {}", options.v8_target_cpu)
  for _, stdout, _ in invoke_clang_plugin_for_each_file(files, "dump-callees",
                                                        [], options):
    collector.parse(stdout.splitlines())
  collector.propagate()
  # TODO(cbruni): remove once gcmole.cc is migrated
  write_gcmole_results(collector, options, options.v8_root_dir)
  write_gcmole_results(collector, options, options.out_dir)


def write_gcmole_results(collector, options, dst):
  # gcsuspects contains a list("mangled_full_name,name") of all functions that
  # could cause a gc (directly or indirectly).
  #
  # EXAMPLE
  # _ZN2v88internal4Heap16CreateApiObjectsEv,CreateApiObjects
  # _ZN2v88internal4Heap17CreateInitialMapsEv,CreateInitialMaps
  # ...
  with open(dst / "gcsuspects", "w") as out:
    for name, value in list(collector.gc.items()):
      if value:
        out.write(name + "\n")
  # gccauses contains a map["mangled_full_name,name"] => list(inner gcsuspects)
  # Where the inner gcsuspects are functions directly called in the outer
  # function that can cause a gc. The format is encoded for simplified
  # deserialization in gcmole.cc.
  #
  # EXAMPLE:
  # _ZN2v88internal4Heap17CreateHeapObjectsEv,CreateHeapObjects
  # start,nested
  # _ZN2v88internal4Heap16CreateApiObjectsEv,CreateApiObjects
  # _ZN2v88internal4Heap17CreateInitialMapsEv,CreateInitialMaps
  # ...
  # end,nested
  # ...
  with open(dst / "gccauses", "w") as out:
    for name, causes in list(collector.gc_caused.items()):
      out.write("{}\n".format(name))
      out.write("start,nested\n")
      for cause in causes:
        out.write("{}\n".format(cause))
      out.write("end,nested\n")
  log("GCSuspects and gccauses generated for {} in '{}'", options.v8_target_cpu,
      dst)


# ------------------------------------------------------------------------------
# Analysis


def check_correctness_for_arch(options, for_test):
  files = build_file_list(options, for_test)

  if not options.reuse_gcsuspects:
    generate_gc_suspects(files, options)
  else:
    log("Reusing GCSuspects for {}", options.v8_target_cpu)

  processed_files = 0
  errors_found = False
  output = ""

  log("Searching for evaluation order problems " +
      (' and dead variables' if options.dead_vars else '') + "for" +
      options.v8_target_cpu)
  plugin_args = []
  if options.dead_vars:
    plugin_args.append("--dead-vars")
  if options.verbose:
    plugin_args.append("--verbose")
  if options.verbose_trace:
    plugin_args.append("--verbose-trace")
  for _, _, stderr in invoke_clang_plugin_for_each_file(files, "find-problems",
                                                        plugin_args, options):
    processed_files = processed_files + 1
    if not errors_found:
      errors_found = re.search("^[^:]+:\d+:\d+: (warning|error)", stderr,
                               re.MULTILINE) is not None
    if for_test:
      output = output + stderr
    else:
      sys.stdout.write(stderr)

  log("Done processing {} files.", processed_files)
  log("Errors found" if errors_found else "No errors found")

  return errors_found, output


def test_run(options):
  if not options.test_run:
    return True
  log("Test Run")
  errors_found, output = check_correctness_for_arch(options, True)
  if not errors_found:
    log("Test file should produce errors, but none were found. Output:")
    print(output)
    return False

  new_file = options.out_dir / "test-expectations-gen.txt"
  with open(new_file, "w") as f:
    f.write(output)
  log("Wrote test-results: {}", new_file)

  expected_file = options.v8_root_dir / "tools/gcmole/test-expectations.txt"
  with open(expected_file) as exp_file:
    expectations = exp_file.read()

  if output != expectations:
    diff_file = options.out_dir / "test_output.diff"
    print("#" * 79)
    log("Output mismatch from running tests.")
    log("Please run gcmole manually with --test-run --verbose.")
    log("Expected: " + expected_file)
    log("New:      " + new_file)
    log("*Diff:*   " + diff_file)
    print("#" * 79)
    for line in difflib.unified_diff(
        expectations.splitlines(),
        output.splitlines(),
        fromfile=str(new_file),
        tofile=str(diff_file),
        lineterm="",
    ):
      print(line)

    print("#" * 79)
    log("Full output")
    log("Expected: " + expected_file)
    log("Diff:     " + diff_file)
    log("*New:*    " + new_file)
    print("#" * 79)
    print(output)
    print("#" * 79)

    return False

  log("Tests ran successfully")
  return True


# =============================================================================
def relative_parents(path, level=0):
  return Path(os.path.relpath(str(path.resolve().parents[level])))


def main(args):
  # Print arguments for better debugging on the bots
  # Get a clean parent path relative to PWD
  gcmole_dir = relative_parents(Path(args[0]))

  parser = optparse.OptionParser()
  archs = list(ARCHITECTURES.keys())
  parser.add_option(
      "--v8-target-cpu",
      type="choice",
      choices=archs,
      help="Tested CPU architecture. Choices: {}".format(archs),
      metavar="CPU")
  default_clang_bin_dir = gcmole_dir / 'gcmole-tools/bin'
  parser.add_option(
      "--clang-bin-dir",
      metavar="DIR",
      help="Build dir of the custom clang version for gcmole." + \
      "Default: env['CLANG_DIR'] or '{}'".format(default_clang_bin_dir))
  parser.add_option(
      "--clang-plugins-dir",
      metavar="DIR",
      help="Containing dir for libgcmole.so."
      "Default: env['CLANG_PLUGINS'] or '{}'".format(gcmole_dir))
  default_root_dir = relative_parents(gcmole_dir, 1)
  parser.add_option(
      "--v8-root-dir",
      metavar="DIR",
      default=default_root_dir,
      help="V8 checkout directory. Default: '{}'".format(default_root_dir))
  parser.add_option(
      "--v8-build-dir",
      metavar="BUILD_DIR",
      help="GN build dir for v8. Default: 'out/CPU.Release'. "
      "Config must match cpu specified by --v8-target-cpu")
  parser.add_option(
      "--out-dir",
      metavar="DIR",
      help="Output location for the gcsuspect and gcauses file."
      "Default: BUILD_DIR/gen/tools/gcmole")
  parser.add_option(
      "--is-bot",
      action="store_true",
      default=False,
      help="Flag for setting build bot specific settings.")

  group = optparse.OptionGroup(parser, "GCMOLE options")
  group.add_option(
      "--reuse-gcsuspects",
      action="store_true",
      default=False,
      help="Don't build gcsuspects file and reuse previously generated one.")
  group.add_option(
      "--sequential",
      action="store_true",
      default=False,
      help="Don't use parallel python runner.")
  group.add_option(
      "--verbose",
      action="store_true",
      default=False,
      help="Print commands to console before executing them.")
  group.add_option(
      "--no-dead-vars",
      action="store_false",
      dest="dead_vars",
      default=True,
      help="Don't perform dead variable analysis.")
  group.add_option(
      "--verbose-trace",
      action="store_true",
      default=False,
      help="Enable verbose tracing from the plugin itself."
      "This can be useful to debug finding dead variable.")
  group.add_option(
      "--no-allowlist",
      action="store_true",
      default=True,
      dest="allowlist",
      help="When building gcsuspects allowlist certain functions as if they can be "
      "causing GC. Currently used to reduce number of false positives in dead "
      "variables analysis. See TODO for ALLOWLIST in gcmole.py")
  group.add_option(
      "--test-run",
      action="store_true",
      default=False,
      help="Test gcmole on tools/gcmole/gcmole-test.cc")
  parser.add_option_group(group)

  (options, args) = parser.parse_args()

  if not options.v8_target_cpu:
    # Backwards compatibility
    if len(args[0]) > 0 and args[0] in archs:
      options.v8_target_cpu = args[0]
      log("Using --v8-target-cpu={}", options.v8_target_cpu)
    else:
      parser.error("Missing --v8-target-cpu option")

  options.is_bot = False
  verify_and_convert_dirs(parser, options, gcmole_dir, default_clang_bin_dir)
  verify_clang_plugin(parser, options)
  prepare_gcmole_files(options)
  verify_build_config(parser, options)

  any_errors_found = False
  if not test_run(options):
    any_errors_found = True
  else:
    errors_found, output = check_correctness_for_arch(options, False)
    any_errors_found = any_errors_found or errors_found

  sys.exit(1 if any_errors_found else 0)


def verify_and_convert_dirs(parser, options, gcmole_dir, default_clang_bin_dir):
  # Verify options for setting directors and convert the input strings to Path
  # objects.
  options.v8_root_dir = Path(options.v8_root_dir)

  if not options.clang_bin_dir:
    if os.getenv("CLANG_BIN"):
      options.clang_bin_dir = Path(os.getenv("CLANG_BIN"))
      options.is_bot = True
    else:
      options.clang_bin_dir = default_clang_bin_dir
      if not (options.clang_bin_dir / 'clang++').exists():
        options.clang_bin_dir = Path(gcmole_dir,
                                     "tools/gcmole/bootstrap/build/bin")
    log("Using --clang-bin-dir={}", options.clang_bin_dir)
  else:
    options.clang_bin_dir = Path(options.clang_bin_dir)

  if not options.clang_plugins_dir:
    if os.getenv("CLANG_PLUGINS"):
      options.clang_plugins_dir = Path(os.getenv("CLANG_PLUGINS"))
    else:
      options.clang_plugins_dir = gcmole_dir.resolve()
    log("Using --clang-plugins-dir={}", options.clang_plugins_dir)
  else:
    options.clang_plugins_dir = Path(options.clang_plugins_dir)

  if not options.v8_build_dir:
    config = ARCHITECTURES[options.v8_target_cpu]
    options.v8_build_dir = options.v8_root_dir / ('out/%s.release' %
                                                  config.name)
    # Fallback for build bots.
    if not options.v8_build_dir.exists() and os.getenv("CLANG_BIN"):
      options.v8_build_dir = options.v8_root_dir / 'out/build'
    log("Using --v8-build-dir={}", options.v8_build_dir)
  else:
    options.v8_build_dir = Path(options.v8_build_dir)

  if not options.out_dir:
    options.out_dir = options.v8_build_dir / 'gen/tools/gcmole'
    if options.v8_build_dir.exists():
      options.out_dir.mkdir(parents=True, exist_ok=True)
  else:
    options.out_dir = Path(options.out_dir)

  for flag in [
      "--v8-root-dir", "--v8-build-dir", "--clang-bin-dir",
      "--clang-plugins-dir", "--out-dir"
  ]:
    dir = getattr(options, parser.get_option(flag).dest)
    if not dir.is_dir():
      parser.error("{}='{}' does not exist!".format(flag, dir))


def verify_clang_plugin(parser, options):
  libgcmole_path = options.clang_plugins_dir / "libgcmole.so"
  if not libgcmole_path.is_file():
    parser.error("'{}' does not exist. Please build gcmole first.".format(
        libgcmole_path))
  clang_path = options.clang_bin_dir / "clang++"
  if not clang_path.is_file():
    parser.error(
        "'{}' does not exist. Please build gcmole first.".format(clang_path))


def prepare_gcmole_files(options):
  cmd = [
      "ninja", "-C", options.v8_build_dir, "v8_gcmole_files",
      "v8_dump_build_config"
  ]
  cmd = list(map(str, cmd))
  log("Preparing files: {}", " ".join(cmd))
  try:
    subprocess.check_call(cmd)
  except:
    # Ignore ninja task errors on the bots
    if options.is_bot:
      log("Ninja command failed, ignoring errors.")
    else:
      raise


def verify_build_config(parser, options):
  if options.is_bot:
    #TODO(cbruni): Fix, currently not supported on the bots
    return
  config_file = options.v8_build_dir / 'v8_build_config.json'
  with open(config_file) as f:
    config = json.load(f)
  found_cpu = None
  for key in ('v8_target_cpu', 'target_cpu', 'current_cpu'):
    found_cpu = config.get('v8_target_cpu')
    if found_cpu == options.v8_target_cpu:
      return
  parser.error("Build dir '{}' config doesn't match request cpu. {}: {}".format(
      options.v8_build_dir, options.v8_target_cpu, found_cpu))


if __name__ == "__main__":
  main(sys.argv)
