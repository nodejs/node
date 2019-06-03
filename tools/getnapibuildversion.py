from __future__ import print_function
import os
import re


def get_napi_version():
  napi_version_h = os.path.join(
    os.path.dirname(__file__),
    '..',
    'src',
    'js_native_api.h')

  f = open(napi_version_h)

  regex = '^#define NAPI_VERSION [0-9]+'

  for line in f:
    if re.match(regex, line):
      napi_version = line.split()[2]
      return napi_version

  raise Exception('Could not find pattern matching %s' % regex)


if __name__ == '__main__':
  print(get_napi_version())
