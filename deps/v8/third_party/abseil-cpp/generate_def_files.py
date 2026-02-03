#!/usr/bin/env python3

"""Script to generate Chromium's Abseil .def files at roll time.

This script generates //third_party/abseil-app/absl/symbols_*.def at Abseil
roll time.

Since Abseil doesn't export symbols, Chromium is forced to consider all
Abseil's symbols as publicly visible. On POSIX it is possible to use
-fvisibility=default but on Windows a .def file with all the symbols
is needed.

Unless you are on a Windows machine, you need to set up your Chromium
checkout for cross-compilation by following the instructions at
https://chromium.googlesource.com/chromium/src.git/+/main/docs/win_cross.md.
If you are on Windows, you may need to tweak this script to run, e.g. by
changing "gn" to "gn.bat", changing "llvm-nm" to the name of your copy of
llvm-nm, etc.
"""

import fnmatch
import logging
import os
import re
import subprocess
import sys
import tempfile
import time


assert sys.platform != 'win32', \
  "This doesn't work on Windows, https://crbug.com/379023022"


# Matches mangled symbols containing 'absl' or starting with 'Absl'. This is
# a good enough heuristic to select Abseil symbols to list in the .def file.
# See https://learn.microsoft.com/en-us/cpp/build/reference/decorated-names,
# which describes decorations under different calling conventions. We mostly
# just attempt to handle any leading underscore for C names (as in __cdecl).
ABSL_SYM_RE = r'0* [BT] (?P<symbol>[?]+[^?].*absl.*|_?Absl.*)'


def _DebugOrRelease(is_debug):
  return 'dbg' if is_debug else 'rel'


def _GenerateDefFileBuild(cpu, is_debug, use_cxx23, extra_gn_args, suffix, out_dir, cwd):
  if extra_gn_args:
    assert suffix != None, 'suffix is needed when extra_gn_args is used'

  flavor = _DebugOrRelease(is_debug)
  gn_args = [
      'ffmpeg_branding = "Chrome"',
      'is_component_build = true',
      'is_debug = {}'.format(str(is_debug).lower()),
      'proprietary_codecs = true',
      'use_cxx23={}'.format(str(use_cxx23).lower()),
      'symbol_level = 0',
      'target_cpu = "{}"'.format(cpu),
      'target_os = "win"',
      'use_remoteexec = true',
  ]
  gn_args.extend(extra_gn_args)

  gn = 'gn'
  autoninja = 'autoninja'
  llvm_nm = ['third_party/llvm-build/Release+Asserts/bin/llvm-nm']
  if sys.platform == 'win32':
    gn = 'gn.bat'
    autoninja = 'autoninja.bat'
    llvm_nm += '.exe'

  logging.info('[%s - %s] Creating tmp out dir in %s', cpu, flavor, out_dir)
  subprocess.check_call([gn, 'gen', out_dir, '--args=' + ' '.join(gn_args)],
                        cwd=cwd)
  logging.info('[%s - %s] gn gen completed', cpu, flavor)
  subprocess.check_call(
      [autoninja, '-C', out_dir, 'third_party/abseil-cpp:absl_component_deps'],
      cwd=os.getcwd())
  logging.info('[%s - %s] autoninja completed', cpu, flavor)

  obj_files = []
  for root, _dirnames, filenames in os.walk(
      os.path.join(out_dir, 'obj', 'third_party', 'abseil-cpp')):
    matched_files = fnmatch.filter(filenames, '*.obj')
    obj_files.extend((os.path.join(root, f) for f in matched_files))

  logging.info('[%s - %s] Found %d object files.', cpu, flavor, len(obj_files))

  absl_symbols = set()
  for f in obj_files:
    stdout = subprocess.check_output(llvm_nm + [f], cwd=os.getcwd())
    for line in stdout.splitlines():
      line = line.decode('utf-8')
      match = re.match(ABSL_SYM_RE, line)
      if match:
        symbol = match.group('symbol')
        assert symbol.count(' ') == 0, ('Regex matched too much, probably got '
                                        'undecorated name as well')
        # Avoid to export deleting dtors since they trigger
        # "lld-link: error: export of deleting dtor" linker errors, see
        # crbug.com/1201277.
        if symbol.startswith('??_G'):
          continue
        # Strip any leading underscore for C names (as in __cdecl). It's only
        # there on x86, but the x86 toolchain falls over when you include it!
        if cpu == 'x86' and symbol.startswith('_'):
          symbol = symbol[1:]
        absl_symbols.add(symbol)

  logging.info('[%s - %s] Found %d absl symbols.', cpu, flavor, len(absl_symbols))

  if extra_gn_args:
    def_file = os.path.join('third_party', 'abseil-cpp',
                            'symbols_{}_{}_{}'.format(cpu, flavor, suffix))
  else:
    def_file = os.path.join('third_party', 'abseil-cpp',
                           'symbols_{}_{}'.format(cpu, flavor))
  if use_cxx23:
    def_file += "_cxx23"
  def_file += ".def"

  with open(def_file, 'w', newline='') as f:
    f.write('EXPORTS\n')
    for s in sorted(absl_symbols):
      f.write('    {}\n'.format(s))

  logging.info('[%s - %s] .def file successfully generated.', cpu, flavor)


def _GenerateDefFile(cpu, is_debug, use_cxx23, extra_gn_args=[], suffix=None):
  """Generates a .def file for the absl component build on the specified CPU."""
  cwd = os.getcwd()
  with tempfile.TemporaryDirectory(dir=os.path.join(cwd, 'out')) as out_dir:
    _GenerateDefFileBuild(cpu, is_debug, use_cxx23, extra_gn_args, suffix, out_dir, cwd)

    # Hack, it looks like there is a race in the directory cleanup.
    time.sleep(10)


if __name__ == '__main__':
  logging.getLogger().setLevel(logging.INFO)

  if not os.getcwd().endswith('src') or not os.path.exists('chrome/browser'):
    logging.error('Run this script from a chromium/src/ directory.')
    exit(1)

  for use_cxx23 in (True, False):
    _GenerateDefFile('x64', False, use_cxx23, ['is_asan = true'], 'asan')
    for arch in ('x86', 'x64', 'arm64'):
      for is_debug in (True, False):
          _GenerateDefFile(arch, is_debug, use_cxx23)
