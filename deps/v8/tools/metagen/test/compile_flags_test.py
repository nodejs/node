#!/usr/bin/env python3
# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

# Only this file's directory is on sys.path when it runs as a script,
# so the `metagen` package is not importable yet. Import through it
# anyway, so the patches below land on the module object metagen.py
# holds.
_TOOLS = os.path.dirname(
    os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
if _TOOLS not in sys.path:
  sys.path.insert(0, _TOOLS)

from metagen import compile_flags  # noqa: E402


class GnCompileFlagsTest(unittest.TestCase):

  def test_external_build_directory(self):
    with tempfile.TemporaryDirectory() as tmp:
      source_root = Path(tmp) / 'src'
      build_dir = Path(tmp) / 'cache' / 'Release'
      source_root.mkdir()
      build_dir.mkdir(parents=True)
      (source_root / '.gn').touch()
      target = '//v8:probe(//build/toolchain:target)'
      desc = {
          target: {
              'defines': ['PROBE=1'],
              'include_dirs': ['//v8/include',
                               str(build_dir / 'gen')],
              'cflags_cc': ['-std=c++20'],
          }
      }
      with mock.patch.object(
          compile_flags, '_find_gn',
          return_value='gn') as find_gn, mock.patch.object(
              compile_flags.subprocess,
              'run',
              return_value=subprocess.CompletedProcess(
                  [], 0, json.dumps(desc))) as run:
        flags, cwd, cl_mode = compile_flags.get_compile_args_from_gn_desc(
            str(build_dir), target, str(source_root))
      find_gn.assert_called_once_with(str(source_root))
      # gn runs in the source root, and reaches the build dir through a
      # relative path that leaves the checkout.
      argv, kwargs = run.call_args
      self.assertEqual(argv[0], [
          'gn', 'desc', '-q',
          os.path.relpath(build_dir, source_root), target, '--format=json'
      ])
      self.assertEqual(kwargs['cwd'], str(source_root))
      self.assertTrue(kwargs['check'])
      self.assertEqual(flags, [
          '-DPROBE=1', f'-I{source_root / "v8" / "include"}',
          f'-I{build_dir / "gen"}', '-std=c++20'
      ])
      self.assertEqual(cwd, str(build_dir))
      self.assertFalse(cl_mode)


class FilterTest(unittest.TestCase):

  def test_drops_clang_modules_family(self):
    # A use_clang_modules build passes all of these. -fbuiltin-module-map
    # is the easy one to miss: it names no path, so it does nothing until
    # the resource dir actually has builtin headers in it.
    modules_flags = [
        '-fmodules',
        '-fmodule-map-file=/x/module.modulemap',
        '-fmodule-file=std=/x/std.pcm',
        '-fimplicit-modules',
        '-fimplicit-module-maps',
        '-fno-implicit-modules',
        '-fno-implicit-module-maps',
        '-fbuiltin-module-map',
    ]
    self.assertEqual(
        compile_flags._filter(modules_flags + ['-DKEEP'], ''), ['-DKEEP'])


class BazelCompileFlagsTest(unittest.TestCase):
  """The compile_commands.json path (bazel/defs.bzl synthesizes it)."""

  def write(self, tmp, entries):
    path = Path(tmp) / 'compile_commands.json'
    path.write_text(json.dumps(entries))
    return str(path)

  # This entry carries one flag from every category the filter drops.
  ARGUMENTS = [
      'clang++',
      '-c',
      'harvest-driver.cc',
      '-o',
      'driver.o',
      '-MMD',
      '-MF',
      'driver.d',
      '-DV8_ENABLE_SANDBOX',
      '-I../../include',
      '-Xclang',
      '-add-plugin',
      '-mllvm',
      '-instcombine-lower-dbg',
      '-Werror=unused',
      '-fprofile-use=default.profdata',
      '-fmodule-file=std.pcm',
      '-std=c++20',
  ]

  def entry(self, **overrides):
    entry = {
        'directory': '/build',
        'file': 'harvest-driver.cc',
        'arguments': list(self.ARGUMENTS),
    }
    entry.update(overrides)
    return entry

  def test_only_the_parseable_flags_survive(self):
    with tempfile.TemporaryDirectory() as tmp:
      flags, cwd, cl_mode = compile_flags.get_compile_args_from_file(
          self.write(tmp, [self.entry()]))
    # argv[0], the input file, the compile/dep/output flags, the plugin
    # and backend pairs and the instrumentation families all go.
    self.assertEqual(flags,
                     ['-DV8_ENABLE_SANDBOX', '-I../../include', '-std=c++20'])
    self.assertEqual(cwd, '/build')
    self.assertFalse(cl_mode)

  def test_clang_cl_is_detected_from_argv0(self):
    with tempfile.TemporaryDirectory() as tmp:
      entry = self.entry(arguments=['clang-cl.exe', '/std:c++20', '/WX'])
      flags, _, cl_mode = compile_flags.get_compile_args_from_file(
          self.write(tmp, [entry]))
    self.assertTrue(cl_mode)
    # /WX would escalate libclang's own warnings into errors.
    self.assertEqual(flags, ['/std:c++20'])

  def test_response_file_is_rejected(self):
    with tempfile.TemporaryDirectory() as tmp:
      entry = self.entry(arguments=['clang++', '@driver.rsp'])
      with self.assertRaises(RuntimeError) as e:
        compile_flags.get_compile_args_from_file(self.write(tmp, [entry]))
    self.assertIn('response file', str(e.exception))

  def test_a_command_string_is_rejected(self):
    # A `command` string would need the shell tokenization this path
    # deliberately lacks.
    with tempfile.TemporaryDirectory() as tmp:
      entry = self.entry()
      del entry['arguments']
      entry['command'] = 'clang++ -c harvest-driver.cc'
      with self.assertRaises(RuntimeError) as e:
        compile_flags.get_compile_args_from_file(self.write(tmp, [entry]))
    self.assertIn('no `arguments` array', str(e.exception))

  def test_more_than_one_entry_is_rejected(self):
    with tempfile.TemporaryDirectory() as tmp:
      path = self.write(tmp, [self.entry(), self.entry()])
      with self.assertRaises(RuntimeError) as e:
        compile_flags.get_compile_args_from_file(path)
    self.assertIn('one-entry list', str(e.exception))


if __name__ == '__main__':
  unittest.main()
