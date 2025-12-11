from __future__ import print_function
import os


def get_major_minor_patch(text):
  for line in text.splitlines():
    if line.startswith('#define NODE_MAJOR_VERSION'):
      major = line.split()[2]
    elif line.startswith('#define NODE_MINOR_VERSION'):
      minor = line.split()[2]
    elif line.startswith('#define NODE_PATCH_VERSION'):
      patch = line.split()[2]
  return major, minor, patch


node_version_h = os.path.join(os.path.dirname(__file__),
                              '..',
                              'src',
                              'node_version.h')
with open(node_version_h) as in_file:
  print('.'.join(get_major_minor_patch(in_file.read())))
