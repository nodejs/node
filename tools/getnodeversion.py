from __future__ import print_function
import os
import sys


def get_major_minor_patch(text):
  for line in text.splitlines():
    if line.startswith('#define NODE_MAJOR_VERSION'):
      major = line.split()[2]
    elif line.startswith('#define NODE_MINOR_VERSION'):
      minor = line.split()[2]
    elif line.startswith('#define NODE_PATCH_VERSION'):
      patch = line.split()[2]
  return major, minor, patch


def get_alpha_tag(text):
  is_release = False
  alpha = {}
  for line in text.splitlines():
    if line.startswith('#define NODE_VERSION_IS_RELEASE'):
      is_release = line.split()[2] == '1'
    elif line.startswith('#define NODE_ALPHA_MAJOR_VERSION'):
      alpha['major'] = line.split()[2]
    elif line.startswith('#define NODE_ALPHA_MINOR_VERSION'):
      alpha['minor'] = line.split()[2]
    elif line.startswith('#define NODE_ALPHA_PATCH_VERSION'):
      alpha['patch'] = line.split()[2]
  if not is_release or 'major' not in alpha:
    return ''
  return '-alpha.' + '.'.join((alpha['major'], alpha['minor'], alpha['patch']))


node_version_h = os.path.join(os.path.dirname(__file__),
                              '..',
                              'src',
                              'node_version.h')
with open(node_version_h) as in_file:
  text = in_file.read()
  version = '.'.join(get_major_minor_patch(text))
  if '--full' in sys.argv[1:]:
    version += get_alpha_tag(text)
  print(version)
