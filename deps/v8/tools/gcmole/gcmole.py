#!/usr/bin/env python3
# Copyright 2020 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This is main driver for gcmole tool. See README for more details.
# Usage: CLANG_BIN=clang-bin-dir python tools/gcmole/gcmole.py [arm|arm64|ia32|x64]

from contextlib import contextmanager
from contextlib import redirect_stderr
from multiprocessing import cpu_count
from pathlib import Path

import argparse
import collections
import difflib
import io
import json
import os
import pickle
import re
import subprocess
import sys
import threading
import queue


ArchCfg = collections.namedtuple("ArchCfg",
                                 ["name", "cpu", "triple", "arch_options"])

ARCHITECTURES = {
    "ia32":
        ArchCfg(
            name="ia32",
            cpu="x86",
            triple="i586-unknown-linux",
            arch_options=["-m32"],
        ),
    "arm":
        ArchCfg(
            name="arm",
            cpu="arm",
            triple="i586-unknown-linux",
            arch_options=["-m32"],
        ),
    "x64":
        ArchCfg(
            name="x64",
            cpu="x64",
            triple="x86_64-unknown-linux",
            arch_options=[]),
    "arm64":
        ArchCfg(
            name="arm64",
            cpu="arm64",
            triple="x86_64-unknown-linux",
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
  with open(options.v8_build_dir / 'v8_gcmole.args') as f:
    generated_args = f.read().strip().split()

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
  return ([
      options.clang_bin_dir / "clang++",
      "-std=c++20",
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
      "-DV8_GC_MOLE",
  ] + generated_args + arch_cfg.arch_options)


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


def build_file_list(options):
  """Calculates the list of source files to be checked with gcmole.

  The list comprises all files from marked source sections in the
  listed BUILD.gn files. All files preceeded by the following comment and
  until the end of the source section are used:
  ### gcmole(arch) ###
  Where arch can either be all (all architectures) or one of the supported V8
  architectures.

  The structure of these directives is also checked by presubmit via:
  tools/v8_presubmit.py::GCMoleProcessor.

  Returns: List of file paths (of type Path).
  """
  if options.test_run:
    return [options.v8_root_dir / "tools/gcmole/gcmole-test.cc"]
  result = []
  gn_files = [
      ("BUILD.gn", re.compile(r'"([^"]*?\.cc)"'), ""),
      ("test/cctest/BUILD.gn", re.compile(r'"(test-[^"]*?\.cc)"'),
       Path("test/cctest/")),
  ]
  gn_re = re.compile(fr"### gcmole\((all|{options.v8_target_cpu})\) ###(.*?)\]",
                     re.MULTILINE | re.DOTALL)
  for filename, file_pattern, prefix in gn_files:
    path = options.v8_root_dir / filename
    with open(path) as gn_file:
      gn = gn_file.read()
      for _, sources in gn_re.findall(gn):
        for file in file_pattern.findall(sources):
          result.append(options.v8_root_dir / prefix / file)

  # Filter files of current shard if running on multiple hosts.
  def is_in_shard(index):
    return (index % options.shard_count) == options.shard_index

  return [f for i, f in enumerate(result) if is_in_shard(i)]


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
SAFEPOINT_PATTERN = ",.*SafepointSlowPath"
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


def merge_hierarchy_dicts(dict1, dict2):
  """Merges dict2 into dict1 in-place.
  Deduplicates bases, overrides, and virtual methods.
  """
  for class_name, info2 in dict2.items():
    if class_name not in dict1:
      dict1[class_name] = {
          'bases': list(info2.get('bases', [])),
          'overrides': dict(info2.get('overrides', {})),
          'virtual_methods': list(info2.get('virtual_methods', []))
      }
    else:
      info1 = dict1[class_name]
      # Merge bases
      bases1 = info1.setdefault('bases', [])
      for base in info2.get('bases', []):
        if base not in bases1:
          bases1.append(base)
      # Merge overrides
      info1.setdefault('overrides', {}).update(info2.get('overrides', {}))
      # Merge virtual_methods
      vm1 = info1.setdefault('virtual_methods', [])
      for vm in info2.get('virtual_methods', []):
        if vm not in vm1:
          vm1.append(vm)
  return dict1


class CallGraph:

  def __init__(self):
    self.funcs = collections.defaultdict(set)
    self.current_caller = None
    self.class_hierarchies = {}

  def parse(self, lines):
    for funcname in lines:
      if not funcname:
        continue

      if funcname[0] != "\t":
        # Always inserting the current caller makes the serialized version
        # more compact.
        self.funcs[funcname]
        self.current_caller = funcname
      else:
        self.funcs[funcname[1:]].add(self.current_caller)

  def to_file(self, file_name):
    """Store call graph in file 'file_name'."""
    log(f"Writing serialized callgraph to {file_name}")
    with open(file_name, 'wb') as f:
      pickle.dump(self, f)

  @staticmethod
  def from_file(file_name):
    """Restore call graph from file 'file_name'."""
    log(f"Reading serialized callgraph from {file_name}")
    with open(file_name, 'rb') as f:
      return pickle.load(f)

  @staticmethod
  def from_files(*file_names):
    """Merge multiple call graphs from a list of files."""
    callgraph = CallGraph()
    merged_hierarchy = {}
    for file_name in file_names:
      cg = CallGraph.from_file(file_name)
      for callee, callers in cg.funcs.items():
        callgraph.funcs[callee].update(callers)
      if hasattr(cg, 'class_hierarchies'):
        merged_hierarchy = merge_hierarchy_dicts(merged_hierarchy,
                                                 cg.class_hierarchies)
    callgraph.class_hierarchies = merged_hierarchy
    return callgraph


class GCSuspectsCollector:

  def __init__(self, options, funcs):
    self.gc = {}
    self.gc_caused = collections.defaultdict(set)
    self.funcs = funcs
    if options.allowlist:
      self.is_special = IS_SPECIAL_WITH_ALLOW_LIST
    else:
      self.is_special = IS_SPECIAL_WITHOUT_ALLOW_LIST

  def add_cause(self, name, cause):
    self.gc_caused[name].add(cause)

  def resolve(self, name):
    m = self.is_special.search(name)
    if not m:
      return

    if m.group("gc"):
      self.gc[name] = True
      self.add_cause(name, "<GC>")
    elif m.group("safepoint"):
      self.gc[name] = True
      self.add_cause(name, "<Safepoint>")
    elif m.group("allow"):
      self.gc[name] = False

  def propagate(self):
    log("Propagating GC information")

    def mark(funcname):
      for caller in self.funcs.get(funcname, []):
        if caller not in self.gc:
          self.gc[caller] = True
          mark(caller)
        self.add_cause(caller, funcname)

    for funcname in self.funcs:
      self.resolve(funcname)

    for funcname in self.funcs:
      if self.gc.get(funcname, False):
        mark(funcname)


def generate_callgraph(files, options):
  """Construct a (potentially partial) call graph from a subset of
  source files.
  """
  callgraph = CallGraph()

  # Clean up old class hierarchy JSONs
  for p in Path(options.out_dir).glob("class_hierarchy_*.json"):
    p.unlink(missing_ok=True)

  log(f"Building call graph for {options.v8_target_cpu}")
  plugin_args = [f"output-dir:{options.out_dir}"]
  for _, stdout, _ in invoke_clang_plugin_for_each_file(files, "dump-callees",
                                                        plugin_args, options):
    callgraph.parse(stdout.splitlines())

  # Read generated class hierarchy JSONs and package them
  merged_hierarchy = {}
  for p in Path(options.out_dir).glob("class_hierarchy_*.json"):
    try:
      with open(p) as f:
        data = json.load(f)
        merged_hierarchy = merge_hierarchy_dicts(merged_hierarchy, data)
    except Exception as e:
      log(f"Warning: failed to read class hierarchy file {p}: {e}")
  callgraph.class_hierarchies = merged_hierarchy

  return callgraph


class SynthesizedNode:
  """Parses and represents a synthesized devirtualization node in GCMole."""

  def __init__(self, key):
    self.key = key
    self.is_valid = False
    self.type = None  # 'VIRTUAL', 'VIRTUALTHIS', 'SYNTHOVER'
    self.base_method_mangled = None
    self.derived_class_qualified = None
    self.method_name = None

    if "," in key:
      mangled, qualified = key.split(",", 1)
    else:
      mangled = key
      qualified = ""

    def extract_class_name(q):
      raw_class = q.rsplit("::", 1)[0]
      if " " in raw_class:
        return raw_class.split()[-1]
      return raw_class

    if mangled.startswith("VIRTUAL-"):
      self.type = 'VIRTUAL'
      if not qualified:
        return
      self.derived_class_qualified = extract_class_name(qualified)
      prefix = f"VIRTUAL-{self.derived_class_qualified}-"
      if mangled.startswith(prefix):
        self.base_method_mangled = mangled[len(prefix):]
        self.is_valid = True

    elif mangled.startswith("VIRTUALTHIS-"):
      self.type = 'VIRTUALTHIS'
      if not qualified:
        return
      self.derived_class_qualified = extract_class_name(qualified)
      prefix = f"VIRTUALTHIS-{self.derived_class_qualified}-"
      if mangled.startswith(prefix):
        self.base_method_mangled = mangled[len(prefix):]
        self.is_valid = True

    elif mangled.startswith("SYNTHOVER-"):
      self.type = 'SYNTHOVER'
      if not qualified:
        return
      self.derived_class_qualified = extract_class_name(qualified)
      prefix = f"SYNTHOVER-{self.derived_class_qualified}-"
      if mangled.startswith(prefix):
        self.base_method_mangled = mangled[len(prefix):]
        self.is_valid = True

    if self.is_valid and qualified:
      method_sig = qualified.rsplit("::", 1)[1]
      self.method_name = method_sig.split("(")[0].strip()


class ClassNode:
  """Represents a single class in the C++ hierarchy."""

  def __init__(self, name):
    self.name = name
    self.bases = []  # List of base class qualified names (strings)
    self.overrides = {}  # Map of mangled base method -> mangled derived method
    self.json_overrides = {}  # Map of original (JSON-defined) overrides
    self.virtual_methods = set()

  def add_base(self, base_name):
    if base_name not in self.bases:
      self.bases.append(base_name)

  def update_overrides(self, overrides_dict):
    self.overrides.update(overrides_dict)
    self.json_overrides.update(overrides_dict)


class ClassHierarchy:
  """Manages the global C++ class hierarchy tree."""

  def __init__(self):
    self.classes = {}  # Map of qualified class name -> ClassNode
    self.subclasses = collections.defaultdict(
        list)  # Map of parent -> list of derived
    self._descendant_overrides_memo = {}
    self.parent_method = {}

  def get_or_create(self, class_name):
    if class_name not in self.classes:
      self.classes[class_name] = ClassNode(class_name)
    return self.classes[class_name]

  def load_from_data(self, data):
    for class_name, info in data.items():
      node = self.get_or_create(class_name)
      for base in info['bases']:
        node.add_base(base)
        # Register subclass link
        if class_name not in self.subclasses[base]:
          self.subclasses[base].append(class_name)
      node.update_overrides(info['overrides'])
      for parent_m, child_m in info['overrides'].items():
        self.parent_method[child_m] = parent_m
      if 'virtual_methods' in info:
        node.virtual_methods.update(info['virtual_methods'])

  def load_from_json(self, filepath):
    """Merges partial class hierarchy JSON into the global tree."""
    try:
      with open(filepath) as f:
        data = json.load(f)
        self.load_from_data(data)
    except Exception as e:
      log(f"Warning: failed to load {filepath}: {e}")

  def find_root_virtual_method(self, mangled):
    curr = mangled
    visited = set()
    while curr in self.parent_method:
      if curr in visited:
        break
      visited.add(curr)
      curr = self.parent_method[curr]
    return curr

  def normalize_overrides(self):
    for class_name, node in self.classes.items():
      normalized = {}
      for parent_m, child_m in node.overrides.items():
        root_m = self.find_root_virtual_method(parent_m)
        normalized[root_m] = child_m
      node.overrides.update(normalized)
      node.json_overrides.update(normalized)

  def is_subclass(self, derived, base):
    if derived == base:
      return True
    node = self.classes.get(derived)
    if not node:
      return False
    for parent in node.bases:
      if self.is_subclass(parent, base):
        return True
    return False

  def find_descendant_overrides(self, derived_class, active_mangled):
    """Recursively finds all descendants overriding the virtual method (Gap 1)."""
    memo_key = (derived_class, active_mangled)
    if memo_key in self._descendant_overrides_memo:
      return self._descendant_overrides_memo[memo_key]

    results = []
    visited = set()

    def visit(sub, target_mangled):
      if (sub, target_mangled) in visited:
        return
      visited.add((sub, target_mangled))

      node = self.classes.get(sub)
      next_mangled = target_mangled

      # Resolve synthetic overrides to their underlying base methods
      underlying_mangled = target_mangled
      if target_mangled.startswith("SYNTHOVER-"):
        underlying_mangled = target_mangled[len("SYNTHOVER-"):].rsplit("-",
                                                                       1)[1]

      if node and underlying_mangled in node.overrides:
        override = node.overrides[underlying_mangled]
        results.append(override)
        next_mangled = override

      for child in self.subclasses[sub]:
        visit(child, next_mangled)

    for sub in self.subclasses[derived_class]:
      visit(sub, active_mangled)

    self._descendant_overrides_memo[memo_key] = results
    return results

  def propagate_all_overrides(self, mangled_to_keys):
    """Transitively propagates overrides down the class hierarchy DAG."""
    visited = {}

    def get_class_of_mangled(mangled):
      underlying = mangled
      if mangled.startswith("SYNTHOVER-"):
        underlying = mangled[len("SYNTHOVER-"):].rsplit("-", 1)[1]

      keys = mangled_to_keys.get(underlying, [])
      if keys:
        _, qualified = keys[0].split(",", 1)
        return qualified.rsplit("::", 1)[0]
      return None

    def propagate(name):
      if name in visited:
        return visited[name]
      node = self.classes.get(name)
      if not node:
        return {}

      # Initialize self virtual methods as overrides (root mappings)
      for vm in node.virtual_methods:
        if vm not in node.overrides:
          node.overrides[vm] = vm

      original_overrides = dict(node.overrides)

      for base in node.bases:
        base_overrides = propagate(base)
        for base_ov, impl in base_overrides.items():
          if impl in original_overrides:
            node.overrides[base_ov] = original_overrides[impl]
          elif base_ov not in node.overrides:
            node.overrides[base_ov] = impl
          else:
            # Handle Multiple Inheritance dominance!
            existing_impl = node.overrides[base_ov]
            if existing_impl != impl:
              class_existing = get_class_of_mangled(existing_impl)
              class_new = get_class_of_mangled(impl)
              if class_existing and class_new:
                if self.is_subclass(class_new, class_existing):
                  # New impl is more derived, it dominates!
                  node.overrides[base_ov] = impl

      visited[name] = node.overrides
      return node.overrides

    for class_name in list(self.classes.keys()):
      propagate(class_name)


def add_synthesized_edge(callgraph, hierarchy, synth_key,
                         derived_class_qualified, target_name,
                         processed_synth_nodes, worklist, mangled_to_keys):
  # Find all matching base keys for target_name in ObjectVisitor
  matching_base_keys = []
  for k in callgraph.funcs.keys():
    if "," not in k:
      continue
    mangled, qualified = k.split(",", 1)
    if (f"v8::internal::ObjectVisitor::{target_name}(" in qualified or
        f"v8::internal::ObjectVisitor::{target_name} " in qualified):
      matching_base_keys.append(k)
  # Standalone Test Run Fallback: harvest base mangled keys from class hierarchy overrides!
  if not matching_base_keys:
    target_signature = f"{len(target_name)}{target_name}"
    harvested_mangled_keys = set()
    for node in hierarchy.classes.values():
      for base_mangled in node.overrides.keys():
        if target_signature in base_mangled and "ObjectVisitor" in base_mangled:
          harvested_mangled_keys.add(base_mangled)

    for base_mangled in harvested_mangled_keys:
      matching_base_keys.append(
          f"{base_mangled},void v8::internal::ObjectVisitor::{target_name}(...)"
      )
  for base_target_key in matching_base_keys:
    base_target_mangled = base_target_key.split(",", 1)[0]

    # Check if Derived overrides this method
    node = hierarchy.classes.get(derived_class_qualified)
    if node and base_target_mangled in node.overrides:
      # Case A: Overridden. Link directly to the concrete override in T
      override_mangled = node.overrides[base_target_mangled]
      override_keys = mangled_to_keys.get(override_mangled, [])
      override_key = None
      class_tu = derived_class_qualified.rsplit(
          "$", 1)[1] if "$" in derived_class_qualified else ""
      for k in override_keys:
        k_qualified = k.split(",", 1)[1]
        k_class = k_qualified.rsplit("::", 1)[0]
        k_tu = k_class.rsplit("$", 1)[1] if "$" in k_class else ""
        if k_tu == class_tu:
          override_key = k
          break
      if not override_key and override_keys:
        override_key = override_keys[0]

      if override_key:
        callgraph.funcs[override_key].add(synth_key)
    else:
      # Case B: Inherited. Synthesize a child node under Derived's namespace
      dummy_mangled = f"SYNTHOVER-{derived_class_qualified}-{base_target_mangled}"
      base_qualified_signature = base_target_key.split(",", 1)[1]
      inherited_signature = base_qualified_signature.replace(
          "v8::internal::ObjectVisitor::", f"{derived_class_qualified}::")
      child_synth_key = f"{dummy_mangled},{inherited_signature}"

      # Add as callee
      callgraph.funcs[child_synth_key].add(synth_key)
      mangled_to_keys[dummy_mangled].append(child_synth_key)

      # Process the synthesized child recursively
      if child_synth_key not in processed_synth_nodes:
        worklist.append(child_synth_key)


def synthesize_virtual_links(callgraph, options):
  # 1. Load and merge class hierarchies
  hierarchy = ClassHierarchy()
  if hasattr(callgraph, 'class_hierarchies') and callgraph.class_hierarchies:
    log("Loading class hierarchy from packaged data in callgraph")
    hierarchy.load_from_data(callgraph.class_hierarchies)
  else:
    log(f"Loading class hierarchy from {options.out_dir}")
    for filepath in Path(options.out_dir).glob("class_hierarchy_*.json"):
      hierarchy.load_from_json(filepath)

  hierarchy.normalize_overrides()

  # 2. Cache all virtual methods
  all_virtual_methods = set()
  for node in hierarchy.classes.values():
    all_virtual_methods.update(node.overrides.keys())
    all_virtual_methods.update(node.virtual_methods)

  # Build static O(1) lookup maps first!
  mangled_to_keys = collections.defaultdict(list)
  callees_of = collections.defaultdict(list)

  for key, callers in callgraph.funcs.items():
    mangled = key.split(",", 1)[0]
    mangled_to_keys[mangled].append(key)
    for caller in callers:
      callees_of[caller].append(key)

  # Transitively propagate overrides down the class hierarchy DAG (respecting MI dominance)
  hierarchy.propagate_all_overrides(mangled_to_keys)

  # ===========================================================================
  # 1. Helper to get all virtual methods transitively (introduced or inherited)
  # ===========================================================================
  vm_memo = {}

  def get_all_virtual_methods(class_name):
    if class_name in vm_memo:
      return vm_memo[class_name]
    node = hierarchy.classes.get(class_name)
    if not node:
      return set()
    vms = set(node.virtual_methods)
    for base in node.bases:
      vms.update(get_all_virtual_methods(base))
    vm_memo[class_name] = vms
    return vms

  # ===========================================================================
  # 2. Helper to insert synthetic overrides top-down
  # ===========================================================================
  def insert_synthetic_overrides_for(class_name):
    node = hierarchy.classes.get(class_name)
    if not node:
      return

    # Keep a copy of overrides defined directly in JSON (not propagated)
    original_overrides = dict(node.json_overrides)

    inherited_vms = set()
    for base in node.bases:
      inherited_vms.update(get_all_virtual_methods(base))

    for base_vm_mangled in inherited_vms:
      if base_vm_mangled not in original_overrides:
        # C inherits base_vm_mangled and does NOT override it directly.
        # The dominant override is already resolved and propagated into node.overrides!
        ancestor_mangled = node.overrides.get(base_vm_mangled)
        if not ancestor_mangled:
          continue

        # Resolve ancestor key matching the current TU context if ambiguous
        ancestor_keys = mangled_to_keys.get(ancestor_mangled, [])
        ancestor_key = None
        class_tu = class_name.rsplit("$", 1)[1] if "$" in class_name else ""
        for k in ancestor_keys:
          k_qualified = k.split(",", 1)[1]
          k_class = k_qualified.rsplit("::", 1)[0]
          k_tu = k_class.rsplit("$", 1)[1] if "$" in k_class else ""
          if k_tu == class_tu:
            ancestor_key = k
            break
        if not ancestor_key and ancestor_keys:
          ancestor_key = ancestor_keys[0]

        if ancestor_key:
          _, ancestor_qualified = ancestor_key.split(",", 1)
          method_name = ancestor_qualified.rsplit("::", 1)[1]

          synth_mangled = f"SYNTHOVER-{class_name}-{base_vm_mangled}"
          synth_qualified = f"{class_name}::{method_name}"
          synth_key = f"{synth_mangled},{synth_qualified}"

          # Inherit callgraph (copy callees of ancestor, specializing VIRTUALTHIS)
          ancestor_callees = callees_of.get(ancestor_key, [])
          for callee_key in ancestor_callees:
            callee_synth = SynthesizedNode(callee_key)
            if callee_synth.is_valid and callee_synth.type == 'VIRTUALTHIS':
              # Specialize VIRTUALTHIS for the new receiver class_name
              _, callee_qualified = callee_key.split(",", 1)
              callee_method_name = callee_qualified.rsplit("::", 1)[1]

              synth_callee_mangled = f"VIRTUALTHIS-{class_name}-{callee_synth.base_method_mangled}"
              synth_callee_qualified = f"{class_name}::{callee_method_name}"
              synth_callee_key = f"{synth_callee_mangled},{synth_callee_qualified}"

              callgraph.funcs[synth_callee_key].add(synth_key)
              callees_of[synth_key].append(synth_callee_key)
              mangled_to_keys[synth_callee_mangled].append(synth_callee_key)
            else:
              # Non-VIRTUALTHIS callee, link directly
              callgraph.funcs[callee_key].add(synth_key)
              callees_of[synth_key].append(callee_key)

          # Update maps
          mangled_to_keys[synth_mangled].append(synth_key)
          node.overrides[base_vm_mangled] = synth_mangled

  # Resolve classes top-down
  resolved_classes = set()

  def resolve_class(class_name):
    if class_name in resolved_classes:
      return
    node = hierarchy.classes.get(class_name)
    if not node:
      return
    for base in node.bases:
      resolve_class(base)
    insert_synthetic_overrides_for(class_name)
    resolved_classes.add(class_name)

  for class_name in list(hierarchy.classes.keys()):
    resolve_class(class_name)

  # ===========================================================================
  # 3. Expand VIRTUALTHIS calls (link them to resolved targets)
  # ===========================================================================
  for method_key in list(callgraph.funcs.keys()):
    synth_node = SynthesizedNode(method_key)
    if synth_node.is_valid and synth_node.type == 'VIRTUALTHIS':
      class_node = hierarchy.classes.get(synth_node.derived_class_qualified)
      active_mangled = synth_node.base_method_mangled
      if class_node:
        override_mangled = class_node.overrides.get(
            synth_node.base_method_mangled)
        if override_mangled:
          active_mangled = override_mangled

      # Link the VIRTUALTHIS node to active implementation
      active_keys = mangled_to_keys.get(active_mangled, [])
      active_key = None
      # Find key with matching TU suffix
      class_tu = synth_node.derived_class_qualified.rsplit(
          "$", 1)[1] if "$" in synth_node.derived_class_qualified else ""
      for k in active_keys:
        k_qualified = k.split(",", 1)[1]
        k_class = k_qualified.rsplit("::", 1)[0]
        k_tu = k_class.rsplit("$", 1)[1] if "$" in k_class else ""
        if k_tu == class_tu:
          active_key = k
          break
      if not active_key and active_keys:
        active_key = active_keys[0]

      if active_key:
        callgraph.funcs[active_key].add(method_key)

      # Link to descendant overrides (linking to ALL matching keys globally)
      desc_overrides = hierarchy.find_descendant_overrides(
          synth_node.derived_class_qualified, active_mangled)
      for o_mangled in desc_overrides:
        override_keys = mangled_to_keys.get(o_mangled, [])
        for override_key in override_keys:
          callgraph.funcs[override_key].add(method_key)

  # ===========================================================================
  # 4. Resolve virtual callsite nodes (VIRTUAL-Receiver-Method)
  # ===========================================================================
  processed_synth_nodes = set()
  worklist = collections.deque(
      [k for k in callgraph.funcs.keys() if k.startswith("VIRTUAL-")])

  while worklist:
    synth_key = worklist.popleft()
    if synth_key in processed_synth_nodes:
      continue

    synth_node = SynthesizedNode(synth_key)
    if not synth_node.is_valid or synth_node.type != 'VIRTUAL':
      continue

    processed_synth_nodes.add(synth_key)

    # --- TERMINAL VISITOR SINKS RESOLUTION ---
    # Removed: redundant and broken terminal visitor sinks resolution.
    # Bypassing to default CHA correctly resolves active implementations and descendant overrides.

    # --- VISITOR MODEL MAP ---
    # Bypasses C++ AST bodies entirely to avoid upcast type loss.
    # We only keep VisitRelocInfo bypass as it is non-virtual and crucial.
    if synth_node.method_name == "VisitRelocInfo":
      for target in [
          "VisitCodeTarget", "VisitEmbeddedPointer", "VisitExternalReference",
          "VisitInternalReference", "VisitOffHeapTarget",
          "VisitJSDispatchTableEntry"
      ]:
        add_synthesized_edge(callgraph, hierarchy, synth_key,
                             synth_node.derived_class_qualified, target,
                             processed_synth_nodes, worklist, mangled_to_keys)
      continue

    # --- DEFAULT CHA DEVIRTUALIZATION ---
    else:
      # Find active implementation at receiver
      class_node = hierarchy.classes.get(synth_node.derived_class_qualified)
      active_mangled = synth_node.base_method_mangled
      if class_node:
        override_mangled = class_node.overrides.get(
            synth_node.base_method_mangled)
        if override_mangled:
          active_mangled = override_mangled

      # Link to active implementation (matching TU suffix)
      active_keys = mangled_to_keys.get(active_mangled, [])
      active_key = None
      class_tu = synth_node.derived_class_qualified.rsplit(
          "$", 1)[1] if "$" in synth_node.derived_class_qualified else ""
      for k in active_keys:
        k_qualified = k.split(",", 1)[1]
        k_class = k_qualified.rsplit("::", 1)[0]
        k_tu = k_class.rsplit("$", 1)[1] if "$" in k_class else ""
        if k_tu == class_tu:
          active_key = k
          break
      if not active_key and active_keys:
        active_key = active_keys[0]

      if active_key:
        callgraph.funcs[active_key].add(synth_key)

      # Link to descendant overrides in subclasses (linking to ALL matching keys globally)
      desc_overrides = hierarchy.find_descendant_overrides(
          synth_node.derived_class_qualified, active_mangled)
      for o_mangled in desc_overrides:
        override_keys = mangled_to_keys.get(o_mangled, [])
        for override_key in override_keys:
          callgraph.funcs[override_key].add(synth_key)


def generate_gc_suspects_from_callgraph(callgraph, options):
  """Calculate and store gc-suspect information from a given call graph."""
  synthesize_virtual_links(callgraph, options)

  collector = GCSuspectsCollector(options, callgraph.funcs)
  collector.propagate()
  # TODO(cbruni): remove once gcmole.cc is migrated
  write_gcmole_results(collector, options, options.v8_root_dir)
  write_gcmole_results(collector, options, options.out_dir)


def generate_gc_suspects_from_files(options):
  """Generate file list and corresponding gc-suspect information."""
  files = build_file_list(options)
  call_graph = generate_callgraph(files, options)
  generate_gc_suspects_from_callgraph(call_graph, options)
  return files


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


def check_correctness_for_arch(files, options):
  processed_files = 0
  errors_found = False

  log("Searching for evaluation order problems " +
      ("and dead variables " if options.dead_vars else "") + "for " +
      options.v8_target_cpu)
  plugin_args = [f"output-dir:{options.out_dir}"]
  if options.dead_vars:
    plugin_args.append("--dead-vars")
  if options.verbose:
    plugin_args.append("--verbose")
  if options.verbose_trace:
    plugin_args.append("--verbose-trace")
  if options.print_gc_call_chain:
    plugin_args.append("--print-gc-call-chain")
  for _, _, stderr in invoke_clang_plugin_for_each_file(files, "find-problems",
                                                        plugin_args, options):
    processed_files = processed_files + 1
    if not errors_found:
      errors_found = re.search(r"^[^:]+:\d+:\d+: (warning|error)", stderr,
                               re.MULTILINE) is not None
    sys.stderr.write(stderr)

  log("Done processing {} files.", processed_files)
  log("Errors found" if errors_found else "No errors found")

  return errors_found


def clean_test_output(output):
  """Substitute line number patterns for files except gcmole-test.cc, as
  otherwise unrelated code changes require a rebaseline of test expectations.
  """

  def clean_block(m):
    # For blocks whose filename is gcmole-test.cc, leave the block unchanged.
    if m.group("file").endswith("gcmole-test.cc"):
      return m.group(0)

    # Otherwise, clear the line numbers both from the main message, and from the
    # individually printed line numbers from the printed source. That is, change
    #
    #   ./src/heap/heap.h:934:21: note: GC call here.
    #      934 |   V8_EXPORT_PRIVATE void CollectGarbage(
    #          |                     ^
    #
    # into
    #
    #   ./src/heap/heap.h:<number>:<number>: note: GC call here.
    #          |   V8_EXPORT_PRIVATE void CollectGarbage(
    #          |                     ^
    block = m.group("block")

    def clear_line_number(m):
      return f"{m.group(1)}{' ' * len(m.group(2))} |{m.group(3)}"

    block = re.sub(r'(\s*)(\d+) \|(.*)', clear_line_number, block)

    return f"{m.group('file')}:<number>:<number>:{m.group('msg')}\n{block}"

  # Substitute within blocks of the form:
  #   ./src/heap/heap.h:934:21: note: GC call here.
  #      934 |   V8_EXPORT_PRIVATE void CollectGarbage(
  #          |           ^
  return re.sub(
      r'(?P<file>\S+):(?P<pos>\d+:\d+):(?P<msg>.*)\n(?P<block>(\s*\d* \|.*\n)*)',
      clean_block, output)

def has_unexpected_errors(options, errors_found, file_io):
  """Returns True if error state isn't as expected, False otherwise.

  In test-run mode, we expect certain errors and return False if expectations
  are met.
  """
  if not options.test_run:
    return errors_found

  log("Test Run")
  output = clean_test_output(file_io.getvalue())
  if not errors_found:
    log("Test file should produce errors, but none were found. Output:")
    print(output)
    return True

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
    log(f"Expected: {expected_file}")
    log(f"New:      {new_file}")
    log(f"*Diff:*   {diff_file}")
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
    log(f"Expected: {expected_file}")
    log(f"Diff:     {diff_file}")
    log(f"*New*:    {new_file}")
    print("#" * 79)
    print(output)
    print("#" * 79)

    return True

  log("Tests ran successfully")
  return False


# =============================================================================
def relative_parents(path, level=0):
  return Path(os.path.relpath(str(path.resolve().parents[level])))


def main(argv):
  # Get a clean parent path relative to PWD
  default_root_dir = relative_parents(Path(__file__), level=2)
  if len(argv) >= 1:
    default_gcmole_dir = relative_parents(Path(argv[0]))
  if default_gcmole_dir or not default_gcmole_dir.exists():
    default_gcmole_dir = default_root_dir / 'tools' / 'gcmole'
  default_clang_bin_dir = default_gcmole_dir / 'gcmole-tools/bin'

  def add_common_args(parser):
    archs = list(ARCHITECTURES.keys())
    parser.add_argument(
        "--v8-root-dir",
        metavar="DIR",
        default=default_root_dir,
        help="V8 checkout directory. Default: '{}'".format(
            default_root_dir.absolute()))
    parser.add_argument(
        "--v8-target-cpu",
        default="x64",
        choices=archs,
        help="Tested CPU architecture. Choices: {}".format(archs),
        metavar="CPU")
    parser.add_argument(
        "--clang-bin-dir",
        metavar="DIR",
        help="Build dir of the custom clang version for gcmole." + \
        "Default: env['CLANG_DIR'] or '{}'".format(default_clang_bin_dir))
    parser.add_argument(
        "--clang-plugins-dir",
        metavar="DIR",
        help="Containing dir for libgcmole.so."
        "Default: env['CLANG_PLUGINS'] or '{}'".format(default_gcmole_dir))
    parser.add_argument(
        "--v8-build-dir",
        metavar="BUILD_DIR",
        help="GN build dir for v8. Default: 'out/CPU.Release'. "
        "Config must match cpu specified by --v8-target-cpu")
    parser.add_argument(
        "--out-dir",
        metavar="DIR",
        help="Output location for the gcsuspect and gcauses file."
        "Default: BUILD_DIR/gen/tools/gcmole")
    parser.add_argument(
        "--is-bot",
        action="store_true",
        default=False,
        help="Flag for setting build bot specific settings.")
    parser.add_argument(
        "--shard-count",
        default=1,
        type=int,
        help="Number of tasks the current action (e.g. collect or check) "
             "is distributed to.")
    parser.add_argument(
        "--shard-index",
        default=0,
        type=int,
        help="Index of the current task (in [0..shard-count-1]) if the "
             "overall action is distributed (shard-count > 1).")

    group = parser.add_argument_group("GCMOLE options")
    group.add_argument(
        "--sequential",
        action="store_true",
        default=False,
        help="Don't use parallel python runner.")
    group.add_argument(
        "--verbose",
        action="store_true",
        default=False,
        help="Print commands to console before executing them.")
    group.add_argument(
        "--no-dead-vars",
        action="store_false",
        dest="dead_vars",
        default=True,
        help="Don't perform dead variable analysis.")
    group.add_argument(
        "--verbose-trace",
        action="store_true",
        default=False,
        help="Enable verbose tracing from the plugin itself."
        "This can be useful to debug finding dead variable.")
    group.add_argument(
        "--no-allowlist",
        action="store_true",
        default=True,
        dest="allowlist",
        help="When building gcsuspects allowlist certain functions as if they can be "
        "causing GC. Currently used to reduce number of false positives in dead "
        "variables analysis. See TODO for ALLOWLIST in gcmole.py")
    group.add_argument(
        "--test-run",
        action="store_true",
        default=False,
        help="Test gcmole on tools/gcmole/gcmole-test.cc")
    group.add_argument(
        "--print-gc-call-chain",
        action="store_true",
        default=False,
        help="Print the complete GC call tree for each reported stale-pointer warning."
    )

  parser = argparse.ArgumentParser()
  subps = parser.add_subparsers()

  subp = subps.add_parser(
      "full", description="Run both gcmole analysis passes.")
  add_common_args(subp)
  subp.set_defaults(func=full_run)

  subp = subps.add_parser(
      "collect",
      description="Construct call graph from source files. "
                  "The action can be distributed using --shard-count and "
                  "--shard-index.")
  add_common_args(subp)
  subp.set_defaults(func=collect_run)
  subp.add_argument(
      "--output",
      required=True,
      help="Path to a file where to store the constructed call graph")

  subp = subps.add_parser(
      "merge",
      description="Merge partial call graphs and propagate gc suspects.")
  add_common_args(subp)
  subp.set_defaults(func=merge_run)
  subp.add_argument(
      "--input",
      action='append',
      required=True,
      help="Path to a file containing a partial call graph stored by "
           "'collect'. Repeat for multiple files.")

  subp = subps.add_parser(
      "check",
      description="Check for problems using previously collected gc-suspect "
                  "information. The action can be distributed using "
                  "--shard-count and --shard-index.")
  add_common_args(subp)
  subp.set_defaults(func=check_run)

  options = parser.parse_args(argv[1:])

  verify_and_convert_dirs(parser, options, default_gcmole_dir,
                          default_clang_bin_dir)
  verify_clang_plugin(parser, options)
  prepare_gcmole_files(options)
  verify_build_config(parser, options)
  override_env_options(options)

  options.func(options)


@contextmanager
def maybe_redirect_stderr(options):
  file_io = io.StringIO() if options.test_run else sys.stderr
  with redirect_stderr(file_io) as f:
    yield f


def check_files(options, files):
  with maybe_redirect_stderr(options) as file_io:
    errors_found = check_correctness_for_arch(files, options)
  sys.exit(has_unexpected_errors(options, errors_found, file_io))


def full_run(options):
  check_files(options, generate_gc_suspects_from_files(options))


def collect_run(options):
  files = build_file_list(options)
  callgraph = generate_callgraph(files, options)
  callgraph.to_file(options.output)


def merge_run(options):
  generate_gc_suspects_from_callgraph(
      CallGraph.from_files(*options.input), options)


def check_run(options):
  check_files(options, build_file_list(options))


def verify_and_convert_dirs(parser, options, default_tools_gcmole_dir,
                            default_clang_bin_dir):
  # Verify options for setting directors and convert the input strings to Path
  # objects.
  options.v8_root_dir = Path(options.v8_root_dir)

  if not options.clang_bin_dir:
    # Backwards compatibility
    if os.getenv("CLANG_BIN"):
      options.clang_bin_dir = Path(os.getenv("CLANG_BIN"))
      options.is_bot = True
    else:
      options.clang_bin_dir = default_clang_bin_dir
      if not (options.clang_bin_dir / 'clang++').exists():
        options.clang_bin_dir = Path(options.v8_root_dir,
                                     "tools/gcmole/bootstrap/build/bin")
    log("Using --clang-bin-dir={}", options.clang_bin_dir)
  else:
    options.clang_bin_dir = Path(options.clang_bin_dir)

  if not options.clang_plugins_dir:
    # Backwards compatibility
    if os.getenv("CLANG_PLUGINS"):
      options.clang_plugins_dir = Path(os.getenv("CLANG_PLUGINS"))
    else:
      options.clang_plugins_dir = default_tools_gcmole_dir.resolve()
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

  for flag, path in [
      ("--v8-root-dir", options.v8_root_dir),
      ("--v8-build-dir", options.v8_build_dir),
      ("--clang-bin-dir", options.clang_bin_dir),
      ("--clang-plugins-dir", options.clang_plugins_dir),
      ("--out-dir", options.out_dir),
  ]:
    if not path.is_dir():
      parser.error(f"{flag}='{path}' does not exist!")


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
      "autoninja", "-C", options.v8_build_dir, "v8_gcmole_files",
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


def override_env_options(options):
  """Set shard options if passed as gtest environment vars on bots."""
  options.shard_count = int(
    os.environ.get('GTEST_TOTAL_SHARDS', options.shard_count))
  options.shard_index = int(
    os.environ.get('GTEST_SHARD_INDEX', options.shard_index))


if __name__ == "__main__":
  main(sys.argv)
