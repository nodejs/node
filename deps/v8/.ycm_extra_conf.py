# Copyright 2015 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Autocompletion config for YouCompleteMe in V8.
#
# USAGE:
#
#   1. Install YCM [https://github.com/Valloric/YouCompleteMe]
#          (Googlers should check out [go/ycm])
#
#   2. Profit
#
#
# Usage notes:
#
#   * You must use ninja & clang to build V8.
#
#   * You must have run gyp_v8 and built V8 recently.
#
#
# Hacking notes:
#
#   * The purpose of this script is to construct an accurate enough command line
#     for YCM to pass to clang so it can build and extract the symbols.
#
#   * Right now, we only pull the -I and -D flags. That seems to be sufficient
#     for everything I've used it for.
#
#   * That whole ninja & clang thing? We could support other configs if someone
#     were willing to write the correct commands and a parser.
#
#   * This has only been tested on gTrusty.


import os
import os.path
import subprocess
import sys


# Flags from YCM's default config.
flags = [
'-DUSE_CLANG_COMPLETER',
'-x',
'c++',
]


def PathExists(*args):
  return os.path.exists(os.path.join(*args))


def FindV8SrcFromFilename(filename):
  """Searches for the root of the V8 checkout.

  Simply checks parent directories until it finds .gclient and v8/.

  Args:
    filename: (String) Path to source file being edited.

  Returns:
    (String) Path of 'v8/', or None if unable to find.
  """
  curdir = os.path.normpath(os.path.dirname(filename))
  while not (PathExists(curdir, 'v8') and PathExists(curdir, 'v8', 'DEPS')
             and (PathExists(curdir, '.gclient')
                  or PathExists(curdir, 'v8', '.git'))):
    nextdir = os.path.normpath(os.path.join(curdir, '..'))
    if nextdir == curdir:
      return None
    curdir = nextdir
  return os.path.join(curdir, 'v8')


def GetClangCommandFromNinjaForFilename(v8_root, filename):
  """Returns the command line to build |filename|.

  Asks ninja how it would build the source file. If the specified file is a
  header, tries to find its companion source file first.

  Args:
    v8_root: (String) Path to v8/.
    filename: (String) Path to source file being edited.

  Returns:
    (List of Strings) Command line arguments for clang.
  """
  if not v8_root:
    return []

  # Generally, everyone benefits from including V8's root, because all of
  # V8's includes are relative to that.
  v8_flags = ['-I' + os.path.join(v8_root)]

  # Version of Clang used to compile V8 can be newer then version of
  # libclang that YCM uses for completion. So it's possible that YCM's libclang
  # doesn't know about some used warning options, which causes compilation
  # warnings (and errors, because of '-Werror');
  v8_flags.append('-Wno-unknown-warning-option')

  # Header files can't be built. Instead, try to match a header file to its
  # corresponding source file.
  if filename.endswith('.h'):
    base = filename[:-6] if filename.endswith('-inl.h') else filename[:-2]
    for alternate in [base + e for e in ['.cc', '.cpp']]:
      if os.path.exists(alternate):
        filename = alternate
        break
    else:
      # If this is a standalone .h file with no source, we ask ninja for the
      # compile flags of some generic cc file ('src/utils/utils.cc'). This
      # should contain most/all of the interesting flags for other targets too.
      filename = os.path.join(v8_root, 'src', 'utils', 'utils.cc')

  sys.path.append(os.path.join(v8_root, 'tools', 'vim'))
  from ninja_output import GetNinjaOutputDirectory
  out_dir = os.path.realpath(GetNinjaOutputDirectory(v8_root))

  # Ninja needs the path to the source file relative to the output build
  # directory.
  rel_filename = os.path.relpath(os.path.realpath(filename), out_dir)

  # Ask ninja how it would build our source file.
  p = subprocess.Popen(['ninja', '-v', '-C', out_dir, '-t',
                        'commands', rel_filename + '^'],
                       stdout=subprocess.PIPE)
  stdout, stderr = p.communicate()
  if p.returncode:
    return v8_flags

  # Ninja might execute several commands to build something. We want the last
  # clang command.
  clang_line = None
  for line in reversed(stdout.decode('utf-8').splitlines()):
    if 'clang' in line:
      clang_line = line
      break
  else:
    return v8_flags

  # Parse flags that are important for YCM's purposes.
  for flag in clang_line.split(' '):
    if flag.startswith('-I'):
      v8_flags.append(MakeIncludePathAbsolute(flag, "-I", out_dir))
    elif flag.startswith('-isystem'):
      v8_flags.append(MakeIncludePathAbsolute(flag, "-isystem", out_dir))
    elif flag.startswith('-std') or flag.startswith(
        '-pthread') or flag.startswith('-no'):
      v8_flags.append(flag)
    elif flag.startswith('-') and flag[1] in 'DWFfmgOX':
      v8_flags.append(flag)
  return v8_flags


def MakeIncludePathAbsolute(flag, prefix, out_dir):
  # Relative paths need to be resolved, because they're relative to the
  # output dir, not the source.
  if flag[len(prefix)] == '/':
    return flag
  else:
    abs_path = os.path.normpath(os.path.join(out_dir, flag[len(prefix):]))
    return prefix + abs_path


def FlagsForFile(filename):
  """This is the main entry point for YCM. Its interface is fixed.

  Args:
    filename: (String) Path to source file being edited.

  Returns:
    (Dictionary)
      'flags': (List of Strings) Command line flags.
      'do_cache': (Boolean) True if the result should be cached.
  """
  v8_root = FindV8SrcFromFilename(filename)
  v8_flags = GetClangCommandFromNinjaForFilename(v8_root, filename)
  final_flags = flags + v8_flags
  return {
    'flags': final_flags,
    'do_cache': True
  }


def Settings(**kwargs):
  if kwargs['language'] == 'cfamily':
    return FlagsForFile(kwargs['filename'])
  return {}
