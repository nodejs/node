#!/usr/bin/env python
# Copyright 2016 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Script to generate V8's gn arguments based on common developer defaults
or builder configurations.

Goma is used by default if a goma folder is detected. The compiler proxy is
assumed to run.

This script can be added to the PATH and be used on other v8 checkouts than
the including one. It always runs for the checkout that nests the CWD.

Configurations of this script live in infra/mb/mb_config.pyl.

-------------------------------------------------------------------------------

Examples:

# Generate the x64.release config in out.gn/x64.release.
v8gen.py x64.release

# Generate into out.gn/foo and disable goma auto-detect.
v8gen.py -b x64.release foo --no-goma

# Pass additional gn arguments after -- (don't use spaces within gn args).
v8gen.py x64.optdebug -- v8_enable_slow_dchecks=true

# Generate gn arguments of 'V8 Linux64 - builder' from 'client.v8'. To switch
# off goma usage here, the args.gn file must be edited manually.
v8gen.py -m client.v8 -b 'V8 Linux64 - builder'

-------------------------------------------------------------------------------
"""

import argparse
import os
import re
import subprocess
import sys

GOMA_DEFAULT = os.path.join(os.path.expanduser("~"), 'goma')
OUT_DIR = 'out.gn'


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
    parser = argparse.ArgumentParser(
      description=__doc__,
      formatter_class=argparse.RawTextHelpFormatter,
    )
    parser.add_argument(
        'outdir', nargs='?',
        help='optional gn output directory')
    parser.add_argument(
        '-b', '--builder',
        help='build configuration or builder name from mb_config.pyl, e.g. '
             'x64.release')
    parser.add_argument(
        '-m', '--master', default='developer_default',
        help='config group or master from mb_config.pyl - default: '
             'developer_default')
    parser.add_argument(
        '-p', '--pedantic', action='store_true',
        help='run gn over command-line gn args to catch errors early')
    parser.add_argument(
        '-v', '--verbosity', action='count',
        help='print wrapped commands (use -vv to print output of wrapped '
             'commands)')

    goma = parser.add_mutually_exclusive_group()
    goma.add_argument(
        '-g' , '--goma',
        action='store_true', default=None, dest='goma',
        help='force using goma')
    goma.add_argument(
        '--nogoma', '--no-goma',
        action='store_false', default=None, dest='goma',
        help='don\'t use goma auto detection - goma might still be used if '
             'specified as a gn arg')

    options = parser.parse_args(args)

    if not options.outdir and not options.builder:
      parser.error('please specify either an output directory or '
                   'a builder/config name (-b), e.g. x64.release')

    if not options.outdir:
      # Derive output directory from builder name.
      options.outdir = _sanitize_nonalpha(options.builder)
    else:
      # Also, if this should work on windows, we might need to use \ where
      # outdir is used as path, while using / if it's used in a gn context.
      if options.outdir.startswith('/'):
        parser.error(
            'only output directories relative to %s are supported' % OUT_DIR)

    if not options.builder:
      # Derive builder from output directory.
      options.builder = options.outdir

    return options

  def verbose_print_1(self, text):
    if self._options.verbosity >= 1:
      print '#' * 80
      print text

  def verbose_print_2(self, text):
    if self._options.verbosity >= 2:
      indent = ' ' * 2
      for l in text.splitlines():
        print indent + l

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
    return True

  def main(self):
    # Always operate relative to the base directory for better relative-path
    # handling. This script can be used in any v8 checkout.
    workdir = self._find_work_dir(os.getcwd())
    if workdir != os.getcwd():
      self.verbose_print_1('cd ' + workdir)
      os.chdir(workdir)

    # The directories are separated with slashes in a gn context (platform
    # independent).
    gn_outdir = '/'.join([OUT_DIR, self._options.outdir])

    # Call MB to generate the basic configuration.
    self._call_cmd([
      sys.executable,
      '-u', os.path.join('tools', 'mb', 'mb.py'),
      'gen',
      '-f', os.path.join('infra', 'mb', 'mb_config.pyl'),
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

if __name__ == "__main__":
  gen = GenerateGnArgs(sys.argv[1:])
  try:
    sys.exit(gen.main())
  except Exception:
    if gen._options.verbosity < 2:
      print ('\nHint: You can raise verbosity (-vv) to see the output of '
             'failed commands.\n')
    raise
