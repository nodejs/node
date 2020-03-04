from __future__ import print_function
import os
import re


def get_napi_version():
  napi_version_h = os.path.join(
    os.path.dirname(__file__),
    '..',
    'src',
    'node_version.h')

  f = open(napi_version_h)

  regex = r'^#\s*define NAPI_VERSION'

  for line in f:
    if re.match(regex, line):
      for i in range(len(line.split())):
        if re.match(r'^.*define', line.split()[i]):
          napi_version = line.split()[i + 2]
          return napi_version

  raise Exception('Could not find pattern matching %s' % regex)


if __name__ == '__main__':
  print(get_napi_version())
