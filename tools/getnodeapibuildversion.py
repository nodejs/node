from __future__ import print_function
import os
import re


def get_node_api_version():
  node_api_version_h = os.path.join(
    os.path.dirname(__file__),
    '..',
    'src',
    'node_version.h')

  f = open(node_api_version_h)

  regex = '^#define NAPI_VERSION'

  for line in f:
    if re.match(regex, line):
      node_api_version = line.split()[2]
      return node_api_version

  raise Exception('Could not find pattern matching %s' % regex)


if __name__ == '__main__':
  print(get_node_api_version())
