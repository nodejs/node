#!/usr/bin/python

# Copyright (c) 2011 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import tempfile
import shutil
import subprocess

def TestCommands(commands, files={}, env={}):
  """Run commands in a temporary directory, returning true if they all succeed.
  Return false on failures or if any commands produce output.

  Arguments:
    commands: an array of shell-interpretable commands, e.g. ['ls -l', 'pwd']
              each will be expanded with Python %-expansion using env first.
    files: a dictionary mapping filename to contents;
           files will be created in the temporary directory before running
           the command.
    env: a dictionary of strings to expand commands with.
  """
  tempdir = tempfile.mkdtemp()
  try:
    for name, contents in files.items():
      f = open(os.path.join(tempdir, name), 'wb')
      f.write(contents)
      f.close()
    for command in commands:
      proc = subprocess.Popen(command % env, shell=True,
                              stdout=subprocess.PIPE,
                              stderr=subprocess.STDOUT,
                              cwd=tempdir)
      output = proc.communicate()[0]
      if proc.returncode != 0 or output:
        return False
    return True
  finally:
    shutil.rmtree(tempdir)
  return False


def TestArSupportsT(ar_command='ar', cc_command='cc'):
  """Test whether 'ar' supports the 'T' flag."""
  return TestCommands(['%(cc)s -c test.c',
                       '%(ar)s crsT test.a test.o',
                       '%(cc)s test.a'],
                      files={'test.c': 'int main(){}'},
                      env={'ar': ar_command, 'cc': cc_command})


def TestLinkerSupportsThreads(cc_command='cc'):
  """Test whether the linker supports the --threads flag."""
  return TestCommands(['%(cc)s -Wl,--threads test.c'],
                      files={'test.c': 'int main(){}'},
                      env={'cc': cc_command})


def TestLinkerSupportsICF(cc_command='cc'):
  """Test whether the linker supports identical code folding."""
  return TestCommands(['%(cc)s -Wl,--icf=safe test.c'],
                      files={'test.c': 'int main(){}'},
                      env={'cc': cc_command})


if __name__ == '__main__':
  # Run the various test functions and print the results.
  def RunTest(description, function, **kwargs):
    print "Testing " + description + ':',
    if function(**kwargs):
      print 'ok'
    else:
      print 'fail'
  RunTest("ar 'T' flag", TestArSupportsT)
  RunTest("ar 'T' flag with ccache", TestArSupportsT, cc_command='ccache cc')
  RunTest("ld --threads", TestLinkerSupportsThreads)
