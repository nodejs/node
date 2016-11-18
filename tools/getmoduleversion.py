from __future__ import print_function
import os
import re

def get_version():
  node_version_h = os.path.join(
    os.path.dirname(__file__),
    '..',
    'src',
    'node_version.h')

  f = open(node_version_h)

  regex = '^#define NODE_MODULE_VERSION [0-9]+'

  for line in f:
    if re.match(regex, line):
      major = line.split()[2]
      return major
  
  raise Exception('Could not find pattern matching %s' % regex)

if __name__ == '__main__':
  print(get_version())
