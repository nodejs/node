#!/usr/bin/env python

import os
import re
import subprocess
import sys


def DoMain(*args):
  cc = os.environ.get('CC', 'gcc')
  stdin, stderr = os.pipe()
  subprocess.call([cc, '-v'], stderr=stderr)
  output = os.read(stdin, 4096)
  match = re.search("\ngcc version (\d+\.\d+\.\d+)", output)
  if match:
    print(match.group(1))


if __name__ == '__main__':
  DoMain(*sys.argv)
