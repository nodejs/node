#!/usr/bin/env python

# Copyright (c) 2013 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from optparse import OptionParser
import glob
import os
import subprocess

parser = OptionParser()
parser.add_option('--exe', dest='exe')
parser.add_option('--vcbindir', dest='vcbindir')
parser.add_option('--pgd', dest='pgd')
(options, args) = parser.parse_args()

# Instrumented binaries fail to run unless the Visual C++'s bin dir is included
# in the PATH environment variable.
os.environ['PATH'] = os.environ['PATH'] + os.pathsep + options.vcbindir

# Run Instrumented binary.  The profile will be recorded into *.pgc file.
subprocess.call([options.exe])

# Merge *.pgc files into a *.pgd (Profile-Guided Database) file.
subprocess.call(['pgomgr', '/merge', options.pgd])

# *.pgc files are no longer necessary. Clear all of them.
pgd_file = os.path.abspath(options.pgd)
pgd_dir = os.path.dirname(pgd_file)
(pgd_basename, _) = os.path.splitext(os.path.basename(pgd_file))
pgc_filepattern = os.path.join(pgd_dir, '%s!*.pgc' % pgd_basename)
pgc_files= glob.glob(pgc_filepattern)
for pgc_file in pgc_files:
  os.unlink(pgc_file)
