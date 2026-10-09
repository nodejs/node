#!/usr/bin/env python3
# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import contextlib
import io
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import types
import unittest
from unittest import mock

# Only this file's directory is on sys.path when it runs as a script.
_TOOLS = os.path.dirname(
    os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
if _TOOLS not in sys.path:
  sys.path.insert(0, _TOOLS)

from metagen import clang_bootstrap, compile_flags, metagen  # noqa: E402


class HarvestReached(Exception):
  pass


class DriverModeTest(unittest.TestCase):

  def test_invalid_mode_arguments(self):
    for options, error in (
        (['--build-dir=out'], 'required: --driver-mode'),
        (['--build-dir=out', '--driver-mode=unknown'], 'invalid choice'),
        (['--compile-commands=commands.json'], 'required: --driver-mode'),
        (['--compile-commands=commands.json',
          '--driver-mode=unknown'], 'invalid choice'),
    ):
      with self.subTest(options=options):
        stderr = io.StringIO()
        with mock.patch.object(sys, 'argv',
                               ['metagen', '--driver=driver.cc', '--out=gen',
                                '--clang-resource-dir=/unused'] +
                               options), contextlib.redirect_stderr(stderr), \
             mock.patch.object(compile_flags,
                               'get_compile_args_from_gn_desc') as gn, \
             mock.patch.object(compile_flags,
                               'get_compile_args_from_file') as database:
          with self.assertRaises(SystemExit) as raised:
            metagen.main()
        self.assertEqual(raised.exception.code, 2)
        self.assertIn(error, stderr.getvalue())
        gn.assert_not_called()
        database.assert_not_called()

  def test_harvest_driver_flags(self):
    for source, mode, raw_flags in (
        ('gn', 'gcc', ['-isysroot', '/SDK', '-iframework', '/Frameworks']),
        ('gn', 'gcc', ['/absolute/path']),
        ('gn', 'cl', []),
        ('gn', 'cl', ['/std:c++20', '/winsysroot', '/SDK']),
        ('clang++', 'gcc', ['-std=c++20']),
        ('clang-cl.exe', 'cl', ['/std:c++20']),
        ('compiler-wrapper', 'gcc', ['-isysroot', '/SDK']),
        ('compiler-wrapper', 'cl', ['/std:c++20']),
        ('x86_64-w64-mingw32-g++', 'gcc', ['-std=c++20']),
    ):
      with self.subTest(source=source, mode=mode, raw_flags=raw_flags), \
           tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        include = root / 'include'
        include.mkdir()
        (include / 'stddef.h').touch()
        argv = [
            'metagen',
            '--driver=driver.cc',
            f'--out={root / "gen"}',
            '--libclang-from-python-env',
            f'--driver-mode={mode}',
            f'--clang-resource-dir={root}',
        ]
        if source == 'gn':
          (root / 'args.gn').touch()
          argv += [
              f'--build-dir={root}',
              f'--source-root={root}',
              '--flags-from-target=//:probe',
              '--flags-toolchain=//:target',
          ]
        else:
          database = root / 'compile_commands.json'
          database.write_text(
              json.dumps([{
                  'directory': str(root),
                  'file': 'probe.cc',
                  'arguments': [source, '-c', 'probe.cc'] + raw_flags,
              }]))
          argv.append(f'--compile-commands={database}')
        desc = {'//:probe(//:target)': {'cflags': raw_flags}}
        scan = mock.Mock(side_effect=HarvestReached)
        cpp_hier = types.ModuleType('metagen.cpp_hier')
        cpp_hier.scan_cpp = scan
        layout_extract = types.ModuleType('metagen.layout_extract')
        with mock.patch.object(sys, 'argv', argv), \
             mock.patch.object(clang_bootstrap, 'bootstrap_from_python_env'), \
             mock.patch.object(sys.modules['metagen'], 'cpp_hier', cpp_hier,
                               create=True), \
             mock.patch.object(sys.modules['metagen'], 'layout_extract',
                               layout_extract, create=True), \
             mock.patch.object(compile_flags, '_find_gn', return_value='gn'), \
             mock.patch.object(compile_flags.subprocess, 'run', return_value=
                               subprocess.CompletedProcess(
                                   [], 0, json.dumps(desc))):
          with self.assertRaises(HarvestReached):
            metagen.main()
        scan.assert_called_once()
        flags = scan.call_args.args[2]
        self.assertEqual(scan.call_args.kwargs['parse_cwd'], str(root))
        self.assertEqual('--driver-mode=cl' in flags, mode == 'cl')
        if mode == 'cl':
          self.assertEqual(flags[0], '--driver-mode=cl')
        self.assertEqual(flags[-1], f'-resource-dir={root}')
        for flag in raw_flags:
          self.assertIn(flag, flags)


if __name__ == '__main__':
  unittest.main()
