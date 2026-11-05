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
      (build_dir / 'args.gn').write_text('target_cpu = "x64"\n')
      target = '//v8:probe(//build/toolchain:target)'
      queried_dir = None

      def run_desc(argv, **kwargs):
        nonlocal queried_dir
        queried_dir = Path(argv[4])
        self.assertNotEqual(queried_dir, build_dir)
        self.assertEqual(queried_dir.parent, build_dir.parent)
        self.assertEqual((queried_dir / 'args.gn').read_text(),
                         'target_cpu = "x64"\n')
        self.assertTrue((queried_dir / 'build.ninja').is_file())
        desc = {
            target: {
                'defines': ['PROBE=1'],
                'include_dirs': [
                    '//v8/include', '//' +
                    os.path.relpath(queried_dir, source_root) + '/gen/include',
                    str(queried_dir / 'gen')
                ],
                'cflags': [
                    '-isysroot', '/SDK', '-isystem',
                    str(queried_dir / 'system/include')
                ],
                'cflags_cc': ['-std=c++20'],
            }
        }
        return subprocess.CompletedProcess(argv, 0, json.dumps(desc))

      with mock.patch.object(compile_flags.sys, 'platform', 'linux'), \
           mock.patch.object(
          compile_flags, '_find_gn',
          return_value='gn') as find_gn, mock.patch.object(
              compile_flags.subprocess, 'run', side_effect=run_desc) as run:
        flags, cwd = compile_flags.get_compile_args_from_gn_desc(
            str(build_dir), target, str(source_root))
      find_gn.assert_called_once_with(str(source_root))
      self.assertFalse(queried_dir.exists())
      argv, kwargs = run.call_args
      self.assertEqual(argv[0], [
          'gn', 'desc', '-q', '--root=' + str(source_root),
          str(queried_dir), target, '--format=json'
      ])
      self.assertEqual(kwargs['cwd'], str(source_root))
      self.assertTrue(kwargs['check'])
      self.assertEqual(flags, [
          '-DPROBE=1', f'-I{source_root / "v8" / "include"}',
          f'-I{build_dir / "gen" / "include"}', f'-I{build_dir / "gen"}',
          '-isysroot', '/SDK', '-isystem',
          str(build_dir / 'system/include'), '-std=c++20'
      ])
      self.assertEqual(cwd, str(build_dir))

  def test_macos_sdk_links_and_override_are_isolated(self):
    with tempfile.TemporaryDirectory() as tmp:
      source_root = Path(tmp) / 'src'
      build_dir = source_root / 'out' / 'Release'
      sdk = Path(tmp) / 'MacOSX.sdk'
      sdk.mkdir()
      sdk_links = build_dir / 'xcode_links' / 'embedder'
      sdk_links.mkdir(parents=True)
      try:
        (sdk_links / sdk.name).symlink_to(sdk, target_is_directory=True)
      except OSError as e:
        if getattr(e, 'winerror', None) != 1314:
          raise
        self.skipTest('Symlink creation requires Windows privileges')
      args = ('mac_sdk_path = '
              '"//out/Release/xcode_links/embedder/MacOSX.sdk"\n')
      (build_dir / 'args.gn').write_text(args)
      queried_dir = None

      def run_desc(argv, **kwargs):
        nonlocal queried_dir
        queried_dir = Path(argv[4])
        query_args = (queried_dir / 'args.gn').read_text()
        self.assertTrue(query_args.startswith(args))
        self.assertIn('if (defined(mac_sdk_path))', query_args)
        query_rel = os.path.relpath(queried_dir, source_root)
        self.assertIn(
            'string_replace(mac_sdk_path, "//out/Release/", '
            f'{json.dumps("//" + query_rel + "/")})', query_args)
        query_sdk = queried_dir / 'xcode_links' / 'embedder' / sdk.name
        self.assertTrue(query_sdk.is_symlink())
        self.assertEqual(query_sdk.resolve(), sdk.resolve())
        # Mutating the query's links must not affect the running build.
        query_sdk.unlink()
        self.assertTrue((sdk_links / sdk.name).is_symlink())
        desc = {'//:probe': {'cflags': ['-isysroot', str(query_sdk)],}}
        return subprocess.CompletedProcess(argv, 0, json.dumps(desc))

      with mock.patch.object(compile_flags.sys, 'platform', 'darwin'), \
           mock.patch.object(compile_flags, '_find_gn', return_value='gn'), \
           mock.patch.object(compile_flags.subprocess, 'run',
                             side_effect=run_desc):
        flags, cwd = compile_flags.get_compile_args_from_gn_desc(
            str(build_dir), '//:probe', str(source_root))
      self.assertEqual(flags, ['-isysroot', str(sdk_links / sdk.name)])
      self.assertEqual(cwd, str(build_dir))
      self.assertEqual((build_dir / 'args.gn').read_text(), args)
      self.assertFalse(queried_dir.exists())

  def test_gn_failure_includes_stdout_and_stderr(self):
    with tempfile.TemporaryDirectory() as tmp:
      build_dir = Path(tmp) / 'build'
      build_dir.mkdir()
      (build_dir / 'args.gn').touch()
      error = subprocess.CalledProcessError(
          1,
          'gn',
          output='File is not inside output directory.',
          stderr='SDK diagnostic')
      with mock.patch.object(compile_flags.sys, 'platform', 'linux'), \
           mock.patch.object(compile_flags, '_find_gn', return_value='gn'), \
           mock.patch.object(compile_flags.subprocess, 'run',
                             side_effect=error):
        with self.assertRaises(RuntimeError) as raised:
          compile_flags.get_compile_args_from_gn_desc(
              str(build_dir), '//:probe', tmp)
      self.assertIn(error.stdout, str(raised.exception))
      self.assertIn(error.stderr, str(raised.exception))

  def test_build_directory_on_another_drive(self):
    with tempfile.TemporaryDirectory() as tmp:
      source_root = Path(tmp) / 'src'
      build_dir = Path(tmp) / 'build'
      source_root.mkdir()
      build_dir.mkdir()
      (build_dir / 'args.gn').touch()

      def run_desc(argv, **kwargs):
        temp_dir = Path(argv[4])
        desc = {'//:probe': {'include_dirs': [str(temp_dir / 'gen')],}}
        return subprocess.CompletedProcess(argv, 0, json.dumps(desc))

      with mock.patch.object(compile_flags.sys, 'platform', 'win32'), \
           mock.patch.object(compile_flags, '_find_gn', return_value='gn'), \
           mock.patch.object(compile_flags.os.path, 'relpath',
                             side_effect=ValueError('different drive')), \
           mock.patch.object(compile_flags.subprocess, 'run',
                             side_effect=run_desc):
        flags, cwd = compile_flags.get_compile_args_from_gn_desc(
            str(build_dir), '//:probe', str(source_root))
      self.assertEqual(flags, [f'-I{build_dir / "gen"}'])
      self.assertEqual(cwd, str(build_dir))


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
      flags, cwd = compile_flags.get_compile_args_from_file(
          self.write(tmp, [self.entry()]))
    # argv[0], the input file, the compile/dep/output flags, the plugin
    # and backend pairs and the instrumentation families all go.
    self.assertEqual(flags,
                     ['-DV8_ENABLE_SANDBOX', '-I../../include', '-std=c++20'])
    self.assertEqual(cwd, '/build')

  def test_clang_cl_flags_survive(self):
    with tempfile.TemporaryDirectory() as tmp:
      entry = self.entry(arguments=['clang-cl.exe', '/std:c++20', '/WX'])
      flags, _ = compile_flags.get_compile_args_from_file(
          self.write(tmp, [entry]))
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
