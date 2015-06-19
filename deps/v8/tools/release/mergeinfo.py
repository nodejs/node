#!/usr/bin/env python
# Copyright 2015 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import os
import sys

from subprocess import call

def print_analysis(gitWorkingDir, hashToSearch):
  print '1.) Info'
  git_execute(gitWorkingDir, ['status'])
  print '2.) Searching for "' + hashToSearch + '"'
  print '=====================ORIGINAL COMMIT START====================='
  git_execute(gitWorkingDir, ['show', hashToSearch])
  print '=====================ORIGINAL COMMIT END====================='
  print '#####################FOUND MERGES & REVERTS START#####################'
  git_execute(gitWorkingDir, ["log",'--all', '--grep='+hashToSearch])
  print '#####################FOUND MERGES & REVERTS END#####################'
  print 'Finished successfully'

def git_execute(workingDir, commands):
  return call(["git", '-C', workingDir] + commands)

if __name__ == "__main__":  # pragma: no cover
  parser = argparse.ArgumentParser('Tool to check where a git commit was merged and reverted.')
  parser.add_argument("-g", "--git-dir", required=False, default='.',
                        help="The path to your git working directory.")

  parser.add_argument('hash', nargs=1, help="Hash of the commit to be searched.")

  args = sys.argv[1:]
  options = parser.parse_args(args)

  sys.exit(print_analysis(options.git_dir, options.hash[0]))
