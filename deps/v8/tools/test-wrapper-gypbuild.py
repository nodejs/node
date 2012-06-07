#!/usr/bin/env python
#
# Copyright 2011 the V8 project authors. All rights reserved.
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
#       copyright notice, this list of conditions and the following
#       disclaimer in the documentation and/or other materials provided
#       with the distribution.
#     * Neither the name of Google Inc. nor the names of its
#       contributors may be used to endorse or promote products derived
#       from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


# This is a convenience script to run the existing tools/test.py script
# when using the gyp/make based build.
# It is intended as a stop-gap rather than a long-term solution.


import optparse
import os
from os.path import join, dirname, abspath
import subprocess
import sys


PROGRESS_INDICATORS = ['verbose', 'dots', 'color', 'mono']


def BuildOptions():
  result = optparse.OptionParser()

  # Flags specific to this wrapper script:
  result.add_option("--arch-and-mode",
                    help='Architecture and mode in the format "arch.mode"',
                    default=None)
  result.add_option("--outdir",
                    help='Base output directory',
                    default='out')
  result.add_option("--no-presubmit",
                    help='Skip presubmit checks',
                    default=False, action="store_true")

  # Flags this wrapper script handles itself:
  result.add_option("-m", "--mode",
                    help="The test modes in which to run (comma-separated)",
                    default='release,debug')
  result.add_option("--arch",
                    help='The architectures to run tests for (comma-separated)',
                    default='ia32,x64,arm')

  # Flags that are passed on to the wrapped test.py script:
  result.add_option("-v", "--verbose", help="Verbose output",
      default=False, action="store_true")
  result.add_option("-p", "--progress",
      help="The style of progress indicator (verbose, dots, color, mono)",
      choices=PROGRESS_INDICATORS, default="mono")
  result.add_option("--report", help="Print a summary of the tests to be run",
      default=False, action="store_true")
  result.add_option("--download-data", help="Download missing test suite data",
      default=False, action="store_true")
  result.add_option("-s", "--suite", help="A test suite",
      default=[], action="append")
  result.add_option("-t", "--timeout", help="Timeout in seconds",
      default=60, type="int")
  result.add_option("--snapshot", help="Run the tests with snapshot turned on",
      default=False, action="store_true")
  result.add_option("--special-command", default=None)
  result.add_option("--valgrind", help="Run tests through valgrind",
      default=False, action="store_true")
  result.add_option("--cat", help="Print the source of the tests",
      default=False, action="store_true")
  result.add_option("--warn-unused", help="Report unused rules",
      default=False, action="store_true")
  result.add_option("-j", help="The number of parallel tasks to run",
      default=1, type="int")
  result.add_option("--time", help="Print timing information after running",
      default=False, action="store_true")
  result.add_option("--suppress-dialogs", help="Suppress Windows dialogs for crashing tests",
        dest="suppress_dialogs", default=True, action="store_true")
  result.add_option("--no-suppress-dialogs", help="Display Windows dialogs for crashing tests",
        dest="suppress_dialogs", action="store_false")
  result.add_option("--isolates", help="Whether to test isolates", default=False, action="store_true")
  result.add_option("--store-unexpected-output",
      help="Store the temporary JS files from tests that fails",
      dest="store_unexpected_output", default=True, action="store_true")
  result.add_option("--no-store-unexpected-output",
      help="Deletes the temporary JS files from tests that fails",
      dest="store_unexpected_output", action="store_false")
  result.add_option("--stress-only",
                    help="Only run tests with --always-opt --stress-opt",
                    default=False, action="store_true")
  result.add_option("--nostress",
                    help="Don't run crankshaft --always-opt --stress-op test",
                    default=False, action="store_true")
  result.add_option("--crankshaft",
                    help="Run with the --crankshaft flag",
                    default=False, action="store_true")
  result.add_option("--shard-count",
                    help="Split testsuites into this number of shards",
                    default=1, type="int")
  result.add_option("--shard-run",
                    help="Run this shard from the split up tests.",
                    default=1, type="int")
  result.add_option("--noprof", help="Disable profiling support",
                    default=False)

  # Flags present in the original test.py that are unsupported in this wrapper:
  # -S [-> scons_flags] (we build with gyp/make, not scons)
  # --no-build (always true)
  # --build-only (always false)
  # --build-system (always 'gyp')
  # --simulator (always true if arch==arm, always false otherwise)
  # --shell (automatically chosen depending on arch and mode)

  return result


def ProcessOptions(options):
  if options.arch_and_mode == ".":
    options.arch = []
    options.mode = []
  else:
    if options.arch_and_mode != None and options.arch_and_mode != "":
      tokens = options.arch_and_mode.split(".")
      options.arch = tokens[0]
      options.mode = tokens[1]
    options.mode = options.mode.split(',')
    options.arch = options.arch.split(',')
  for mode in options.mode:
    if not mode in ['debug', 'release']:
      print "Unknown mode %s" % mode
      return False
  for arch in options.arch:
    if not arch in ['ia32', 'x64', 'arm', 'mips']:
      print "Unknown architecture %s" % arch
      return False

  return True


def PassOnOptions(options):
  result = []
  if options.verbose:
    result += ['--verbose']
  if options.progress != 'mono':
    result += ['--progress=' + options.progress]
  if options.report:
    result += ['--report']
  if options.download_data:
    result += ['--download-data']
  if options.suite != []:
    for suite in options.suite:
      result += ['--suite=../../test/' + suite]
  if options.timeout != 60:
    result += ['--timeout=%s' % options.timeout]
  if options.snapshot:
    result += ['--snapshot']
  if options.special_command:
    result += ['--special-command="%s"' % options.special_command]
  if options.valgrind:
    result += ['--valgrind']
  if options.cat:
    result += ['--cat']
  if options.warn_unused:
    result += ['--warn-unused']
  if options.j != 1:
    result += ['-j%s' % options.j]
  if options.time:
    result += ['--time']
  if not options.suppress_dialogs:
    result += ['--no-suppress-dialogs']
  if options.isolates:
    result += ['--isolates']
  if not options.store_unexpected_output:
    result += ['--no-store-unexpected_output']
  if options.stress_only:
    result += ['--stress-only']
  if options.nostress:
    result += ['--nostress']
  if options.crankshaft:
    result += ['--crankshaft']
  if options.shard_count != 1:
    result += ['--shard-count=%s' % options.shard_count]
  if options.shard_run != 1:
    result += ['--shard-run=%s' % options.shard_run]
  if options.noprof:
    result += ['--noprof']
  return result


def Main():
  parser = BuildOptions()
  (options, args) = parser.parse_args()
  if not ProcessOptions(options):
    parser.print_help()
    return 1

  workspace = abspath(join(dirname(sys.argv[0]), '..'))

  if not options.no_presubmit:
    print ">>> running presubmit tests"
    subprocess.call([workspace + '/tools/presubmit.py'])

  args_for_children = [workspace + '/tools/test.py'] + PassOnOptions(options)
  args_for_children += ['--no-build', '--build-system=gyp']
  for arg in args:
    args_for_children += [arg]
  returncodes = 0
  env = os.environ

  for mode in options.mode:
    for arch in options.arch:
      print ">>> running tests for %s.%s" % (arch, mode)
      shellpath = workspace + '/' + options.outdir + '/' + arch + '.' + mode
      env['LD_LIBRARY_PATH'] = shellpath + '/lib.target'
      shell = shellpath + "/d8"
      child = subprocess.Popen(' '.join(args_for_children +
                                        ['--arch=' + arch] +
                                        ['--mode=' + mode] +
                                        ['--shell=' + shell]),
                               shell=True,
                               cwd=workspace,
                               env=env)
      returncodes += child.wait()

  if len(options.mode) == 0 and len(options.arch) == 0:
    print ">>> running tests"
    shellpath = workspace + '/' + options.outdir
    env['LD_LIBRARY_PATH'] = shellpath + '/lib.target'
    shell = shellpath + '/d8'
    child = subprocess.Popen(' '.join(args_for_children +
                                      ['--shell=' + shell]),
                             shell=True,
                             cwd=workspace,
                             env=env)
    returncodes = child.wait()

  return returncodes


if __name__ == '__main__':
  sys.exit(Main())
