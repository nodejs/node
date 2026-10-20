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

# Running this file as a script puts tools/metagen/ on sys.path, not its
# parent, so the `metagen` package is not importable yet.
_PARENT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if _PARENT not in sys.path:
  sys.path.insert(0, _PARENT)

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


if __name__ == '__main__':
  unittest.main()
