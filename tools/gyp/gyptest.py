#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

__doc__ = """
gyptest.py -- test runner for GYP tests.
"""

import os
import optparse
import shlex
import subprocess
import sys

class CommandRunner(object):
  """
  Executor class for commands, including "commands" implemented by
  Python functions.
  """
  verbose = True
  active = True

  def __init__(self, dictionary={}):
    self.subst_dictionary(dictionary)

  def subst_dictionary(self, dictionary):
    self._subst_dictionary = dictionary

  def subst(self, string, dictionary=None):
    """
    Substitutes (via the format operator) the values in the specified
    dictionary into the specified command.

    The command can be an (action, string) tuple.  In all cases, we
    perform substitution on strings and don't worry if something isn't
    a string.  (It's probably a Python function to be executed.)
    """
    if dictionary is None:
      dictionary = self._subst_dictionary
    if dictionary:
      try:
        string = string % dictionary
      except TypeError:
        pass
    return string

  def display(self, command, stdout=None, stderr=None):
    if not self.verbose:
      return
    if type(command) == type(()):
      func = command[0]
      args = command[1:]
      s = '%s(%s)' % (func.__name__, ', '.join(map(repr, args)))
    if type(command) == type([]):
      # TODO:  quote arguments containing spaces
      # TODO:  handle meta characters?
      s = ' '.join(command)
    else:
      s = self.subst(command)
    if not s.endswith('\n'):
      s += '\n'
    sys.stdout.write(s)
    sys.stdout.flush()

  def execute(self, command, stdout=None, stderr=None):
    """
    Executes a single command.
    """
    if not self.active:
      return 0
    if type(command) == type(''):
      command = self.subst(command)
      cmdargs = shlex.split(command)
      if cmdargs[0] == 'cd':
         command = (os.chdir,) + tuple(cmdargs[1:])
    if type(command) == type(()):
      func = command[0]
      args = command[1:]
      return func(*args)
    else:
      if stdout is sys.stdout:
        # Same as passing sys.stdout, except python2.4 doesn't fail on it.
        subout = None
      else:
        # Open pipe for anything else so Popen works on python2.4.
        subout = subprocess.PIPE
      if stderr is sys.stderr:
        # Same as passing sys.stderr, except python2.4 doesn't fail on it.
        suberr = None
      elif stderr is None:
        # Merge with stdout if stderr isn't specified.
        suberr = subprocess.STDOUT
      else:
        # Open pipe for anything else so Popen works on python2.4.
        suberr = subprocess.PIPE
      p = subprocess.Popen(command,
                           shell=(sys.platform == 'win32'),
                           stdout=subout,
                           stderr=suberr)
      p.wait()
      if stdout is None:
        self.stdout = p.stdout.read()
      elif stdout is not sys.stdout:
        stdout.write(p.stdout.read())
      if stderr not in (None, sys.stderr):
        stderr.write(p.stderr.read())
      return p.returncode

  def run(self, command, display=None, stdout=None, stderr=None):
    """
    Runs a single command, displaying it first.
    """
    if display is None:
      display = command
    self.display(display)
    return self.execute(command, stdout, stderr)


class Unbuffered(object):
  def __init__(self, fp):
    self.fp = fp
  def write(self, arg):
    self.fp.write(arg)
    self.fp.flush()
  def __getattr__(self, attr):
    return getattr(self.fp, attr)

sys.stdout = Unbuffered(sys.stdout)
sys.stderr = Unbuffered(sys.stderr)


def is_test_name(f):
  return f.startswith('gyptest') and f.endswith('.py')


def find_all_gyptest_files(directory):
  result = []
  for root, dirs, files in os.walk(directory):
    if '.svn' in dirs:
      dirs.remove('.svn')
    result.extend([ os.path.join(root, f) for f in files if is_test_name(f) ])
  result.sort()
  return result


def main(argv=None):
  if argv is None:
    argv = sys.argv

  usage = "gyptest.py [-ahlnq] [-f formats] [test ...]"
  parser = optparse.OptionParser(usage=usage)
  parser.add_option("-a", "--all", action="store_true",
            help="run all tests")
  parser.add_option("-C", "--chdir", action="store", default=None,
            help="chdir to the specified directory")
  parser.add_option("-f", "--format", action="store", default='',
            help="run tests with the specified formats")
  parser.add_option("-G", '--gyp_option', action="append", default=[],
            help="Add -G options to the gyp command line")
  parser.add_option("-l", "--list", action="store_true",
            help="list available tests and exit")
  parser.add_option("-n", "--no-exec", action="store_true",
            help="no execute, just print the command line")
  parser.add_option("--passed", action="store_true",
            help="report passed tests")
  parser.add_option("--path", action="append", default=[],
            help="additional $PATH directory")
  parser.add_option("-q", "--quiet", action="store_true",
            help="quiet, don't print test command lines")
  opts, args = parser.parse_args(argv[1:])

  if opts.chdir:
    os.chdir(opts.chdir)

  if opts.path:
    extra_path = [os.path.abspath(p) for p in opts.path]
    extra_path = os.pathsep.join(extra_path)
    os.environ['PATH'] = extra_path + os.pathsep + os.environ['PATH']

  if not args:
    if not opts.all:
      sys.stderr.write('Specify -a to get all tests.\n')
      return 1
    args = ['test']

  tests = []
  for arg in args:
    if os.path.isdir(arg):
      tests.extend(find_all_gyptest_files(os.path.normpath(arg)))
    else:
      if not is_test_name(os.path.basename(arg)):
        print >>sys.stderr, arg, 'is not a valid gyp test name.'
        sys.exit(1)
      tests.append(arg)

  if opts.list:
    for test in tests:
      print test
    sys.exit(0)

  CommandRunner.verbose = not opts.quiet
  CommandRunner.active = not opts.no_exec
  cr = CommandRunner()

  os.environ['PYTHONPATH'] = os.path.abspath('test/lib')
  if not opts.quiet:
    sys.stdout.write('PYTHONPATH=%s\n' % os.environ['PYTHONPATH'])

  passed = []
  failed = []
  no_result = []

  if opts.format:
    format_list = opts.format.split(',')
  else:
    # TODO:  not duplicate this mapping from pylib/gyp/__init__.py
    format_list = {
      'aix5':     ['make'],
      'freebsd7': ['make'],
      'freebsd8': ['make'],
      'openbsd5': ['make'],
      'cygwin':   ['msvs'],
      'win32':    ['msvs', 'ninja'],
      'linux2':   ['make', 'ninja'],
      'linux3':   ['make', 'ninja'],
      'darwin':   ['make', 'ninja', 'xcode', 'xcode-ninja'],
    }[sys.platform]

  for format in format_list:
    os.environ['TESTGYP_FORMAT'] = format
    if not opts.quiet:
      sys.stdout.write('TESTGYP_FORMAT=%s\n' % format)

    gyp_options = []
    for option in opts.gyp_option:
      gyp_options += ['-G', option]
    if gyp_options and not opts.quiet:
      sys.stdout.write('Extra Gyp options: %s\n' % gyp_options)

    for test in tests:
      status = cr.run([sys.executable, test] + gyp_options,
                      stdout=sys.stdout,
                      stderr=sys.stderr)
      if status == 2:
        no_result.append(test)
      elif status:
        failed.append(test)
      else:
        passed.append(test)

  if not opts.quiet:
    def report(description, tests):
      if tests:
        if len(tests) == 1:
          sys.stdout.write("\n%s the following test:\n" % description)
        else:
          fmt = "\n%s the following %d tests:\n"
          sys.stdout.write(fmt % (description, len(tests)))
        sys.stdout.write("\t" + "\n\t".join(tests) + "\n")

    if opts.passed:
      report("Passed", passed)
    report("Failed", failed)
    report("No result from", no_result)

  if failed:
    return 1
  else:
    return 0


if __name__ == "__main__":
  sys.exit(main())
