#!/usr/bin/python

import os
import subprocess
import sys

p = os.path.join(os.environ['BUILT_PRODUCTS_DIR'],os.environ['EXECUTABLE_PATH'])
proc = subprocess.Popen(['codesign', '-v', p],
                        stderr=subprocess.STDOUT, stdout=subprocess.PIPE)
o = proc.communicate()[0].strip()
if "code object is not signed at all" not in o:
  sys.stderr.write('File should not already be signed.')
  sys.exit(1)
