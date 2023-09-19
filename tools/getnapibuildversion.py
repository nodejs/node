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

  regex = '^#define NODE_API_SUPPORTED_VERSION_MAX'

  for line in f:
    if re.match(regex, line):
      napi_version = line.split()[2]
      return napi_version

  raise Exception('Could not find pattern matching %s' % regex)


if __name__ == '__main__':
  print(get_napi_version())
