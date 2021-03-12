#!/usr/bin/env python
# Copyright 2016 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Script to generate V8's gn arguments based on common developer defaults
or builder configurations.

Goma is used by default if detected. The compiler proxy is assumed to run.

This script can be added to the PATH and be used on other checkouts. It always
runs for the checkout nesting the CWD.

Configurations of this script live in infra/mb/mb_config.pyl.

Available actions are: {gen,list}. Omitting the action defaults to "gen".

-------------------------------------------------------------------------------

Examples:

# Generate the ia32.release config in out.gn/ia32.release.
v8gen.py ia32.release

# Generate into out.gn/foo without goma auto-detect.
v8gen.py gen -b ia32.release foo --no-goma

# Pass additional gn arguments after -- (don't use spaces within gn args).
v8gen.py ia32.optdebug -- v8_enable_slow_dchecks=true

# Generate gn arguments of 'V8 Linux64 - builder' from 'client.v8'. To switch
# off goma usage here, the args.gn file must be edited manually.
v8gen.py -m client.v8 -b 'V8 Linux64 - builder'

# Show available configurations.
v8gen.py list

-------------------------------------------------------------------------------
"""

# for py2/py3 compatibility
from __future__ import print_function

import argparse
import os
import re
import subprocess
import sys

CONFIG = os.path.join('infra', 'mb', 'mb_config.pyl')
GOMA_DEFAULT = os.path.join(os.path.expanduser("~"), 'goma')
OUT_DIR = 'out.gn'

TOOLS_PATH = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.append(os.path.join(TOOLS_PATH, 'mb'))

import mb


def _sanitize_nonalpha(text):
  return re.sub(r'[^a-zA-Z0-9.]', '_', text)


class GenerateGnArgs(object):
  def __init__(self, args):
    # Split args into this script's arguments and gn args passed to the
    # wrapped gn.
    index = args.index('--') if '--' in args else len(args)
    self._options = self._parse_arguments(args[:index])
    self._gn_args = args[index + 1:]

  def _parse_arguments(self, args):
    self.parser = argparse.ArgumentParser(
      description=__doc__,
      formatter_class=argparse.RawTextHelpFormatter,
    )

    def add_common_options(p):
      p.add_argument(
          '-m', '--master', default='developer_default',
          help='config group or master from mb_config.pyl - default: '
               'developer_default')
      p.add_argument(
          '-v', '--verbosity', action='count',
          help='print wrapped commands (use -vv to print output of wrapped '
               'commands)')

    subps = self.parser.add_subparsers()

    # Command: gen.
    gen_cmd = subps.add_parser(
        'gen', help='generate a new set of build files (default)')
    gen_cmd.set_defaults(func=self.cmd_gen)
    add_common_options(gen_cmd)
    gen_cmd.add_argument(
        'outdir', nargs='?',
        help='optional gn output directory')
    gen_cmd.add_argument(
        '-b', '--builder',
        help='build configuration or builder name from mb_config.pyl, e.g. '
             'x64.release')
    gen_cmd.add_argument(
        '-p', '--pedantic', action='store_true',
        help='run gn over command-line gn args to catch errors early')

    goma = gen_cmd.add_mutually_exclusive_group()
    goma.add_argument(
        '-g' , '--goma',
        action='store_true', default=None, dest='goma',
        help='force using goma')
    goma.add_argument(
        '--nogoma', '--no-goma',
        action='store_false', default=None, dest='goma',
        help='don\'t use goma auto detection - goma might still be used if '
             'specified as a gn arg')

    # Command: list.
    list_cmd = subps.add_parser(
        'list', help='list available configurations')
    list_cmd.set_defaults(func=self.cmd_list)
    add_common_options(list_cmd)

    # Default to "gen" unless global help is requested.
    if not args or args[0] not in subps.choices.keys() + ['-h', '--help']:
      args = ['gen'] + args

    return self.parser.parse_args(args)

  def cmd_gen(self):
    if not self._options.outdir and not self._options.builder:
      self.parser.error('please specify either an output directory or '
                        'a builder/config name (-b), e.g. x64.release')

    if not self._options.outdir:
      # Derive output directory from builder name.
      self._options.outdir = _sanitize_nonalpha(self._options.builder)
    else:
      # Also, if this should work on windows, we might need to use \ where
      # outdir is used as path, while using / if it's used in a gn context.
      if self._options.outdir.startswith('/'):
        self.parser.error(
            'only output directories relative to %s are supported' % OUT_DIR)

    if not self._options.builder:
      # Derive builder from output directory.
      self._options.builder = self._options.outdir

    # Check for builder/config in mb config.
    if self._options.builder not in self._mbw.builder_groups[self._options.master]:
      print('%s does not exist in %s for %s' % (
          self._options.builder, CONFIG, self._options.master))
      return 1

    # TODO(machenbach): Check if the requested configurations has switched to
    # gn at all.

    # The directories are separated with slashes in a gn context (platform
    # independent).
    gn_outdir = '/'.join([OUT_DIR, self._options.outdir])

    # Call MB to generate the basic configuration.
    self._call_cmd([
      sys.executable,
      '-u', os.path.join('tools', 'mb', 'mb.py'),
      'gen',
      '-f', CONFIG,
      '-m', self._options.master,
      '-b', self._options.builder,
      gn_outdir,
    ])

    # Handle extra gn arguments.
    gn_args_path = os.path.join(OUT_DIR, self._options.outdir, 'args.gn')

    # Append command-line args.
    modified = self._append_gn_args(
        'command-line', gn_args_path, '\n'.join(self._gn_args))

    # Append goma args.
    # TODO(machenbach): We currently can't remove existing goma args from the
    # original config. E.g. to build like a bot that uses goma, but switch
    # goma off.
    modified |= self._append_gn_args(
        'goma', gn_args_path, self._goma_args)

    # Regenerate ninja files to check for errors in the additional gn args.
    if modified and self._options.pedantic:
      self._call_cmd(['gn', 'gen', gn_outdir])
    return 0

  def cmd_list(self):
    print('\n'.join(sorted(self._mbw.builder_groups[self._options.master])))
    return 0

  def verbose_print_1(self, text):
    if self._options.verbosity >= 1:
      print('#' * 80)
      print(text)

  def verbose_print_2(self, text):
    if self._options.verbosity >= 2:
      indent = ' ' * 2
      for l in text.splitlines():
        print(indent + l)

  def _call_cmd(self, args):
    self.verbose_print_1(' '.join(args))
    try:
      output = subprocess.check_output(
        args=args,
        stderr=subprocess.STDOUT,
      )
      self.verbose_print_2(output)
    except subprocess.CalledProcessError as e:
      self.verbose_print_2(e.output)
      raise

  def _find_work_dir(self, path):
    """Find the closest v8 root to `path`."""
    if os.path.exists(os.path.join(path, 'tools', 'dev', 'v8gen.py')):
      # Approximate the v8 root dir by a folder where this script exists
      # in the expected place.
      return path
    elif os.path.dirname(path) == path:
      raise Exception(
          'This appears to not be called from a recent v8 checkout')
    else:
      return self._find_work_dir(os.path.dirname(path))

  @property
  def _goma_dir(self):
    return os.path.normpath(os.environ.get('GOMA_DIR') or GOMA_DEFAULT)

  @property
  def _need_goma_dir(self):
    return self._goma_dir != GOMA_DEFAULT

  @property
  def _use_goma(self):
    if self._options.goma is None:
      # Auto-detect.
      return os.path.exists(self._goma_dir) and os.path.isdir(self._goma_dir)
    else:
      return self._options.goma

  @property
  def _goma_args(self):
    """Gn args for using goma."""
    # Specify goma args if we want to use goma and if goma isn't specified
    # via command line already. The command-line always has precedence over
    # any other specification.
    if (self._use_goma and
        not any(re.match(r'use_goma\s*=.*', x) for x in self._gn_args)):
      if self._need_goma_dir:
        return 'use_goma=true\ngoma_dir="%s"' % self._goma_dir
      else:
        return 'use_goma=true'
    else:
      return ''

  def _append_gn_args(self, type, gn_args_path, more_gn_args):
    """Append extra gn arguments to the generated args.gn file."""
    if not more_gn_args:
      return False
    self.verbose_print_1('Appending """\n%s\n""" to %s.' % (
        more_gn_args, os.path.abspath(gn_args_path)))
    with open(gn_args_path, 'a') as f:
      f.write('\n# Additional %s args:\n' % type)
      f.write(more_gn_args)
      f.write('\n')

    # Artificially increment modification time as our modifications happen too
    # fast. This makes sure that gn is properly rebuilding the ninja files.
    mtime = os.path.getmtime(gn_args_path) + 1
    with open(gn_args_path, 'a'):
      os.utime(gn_args_path, (mtime, mtime))

    return True

  def main(self):
    # Always operate relative to the base directory for better relative-path
    # handling. This script can be used in any v8 checkout.
    workdir = self._find_work_dir(os.getcwd())
    if workdir != os.getcwd():
      self.verbose_print_1('cd ' + workdir)
      os.chdir(workdir)

    # Initialize MB as a library.
    self._mbw = mb.MetaBuildWrapper()

    # TODO(machenbach): Factor out common methods independent of mb arguments.
    self._mbw.ParseArgs(['lookup', '-f', CONFIG])
    self._mbw.ReadConfigFile()

    if not self._options.master in self._mbw.builder_groups:
      print('%s not found in %s\n' % (self._options.master, CONFIG))
      print('Choose one of:\n%s\n' % (
          '\n'.join(sorted(self._mbw.builder_groups.keys()))))
      return 1

    return self._options.func()


if __name__ == "__main__":
  gen = GenerateGnArgs(sys.argv[1:])
  try:
    sys.exit(gen.main())
  except Exception:
    if gen._options.verbosity < 2:
      print ('\nHint: You can raise verbosity (-vv) to see the output of '
             'failed commands.\n')
    raise
