from __future__ import print_function
import os
import re

def get_version():
  package_json = os.path.join(
    os.path.dirname(__file__),
    '..',
    'deps',
    'node-report',
    'package.json')

  f = open(package_json)

  regex = '^\s*\"version\":\s*\"([\d.]+)\"'

  for line in f:
    m = re.match(regex, line)
    if m:
      return m.group(1)
  
  raise Exception('Could not find pattern matching %s' % regex)

if __name__ == '__main__':
  print(get_version())
