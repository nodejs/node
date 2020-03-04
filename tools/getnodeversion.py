from __future__ import print_function
import os
import re

def get_major_minor_patch(text):
  for line in text.splitlines():
    if re.match(r'^#\s*define NODE_MAJOR_VERSION', line):
      for i in range (len(line.split())):
        if re.match(r'^.*define', line.split()[i]):
          major = line.split()[i + 2]
    elif re.match(r'^#\s*define NODE_MINOR_VERSION', line):
      for i in range (len(line.split())):
        if re.match(r'^.*define', line.split()[i]):
          minor = line.split()[i + 2]
    elif re.match(r'^#\s*define NODE_PATCH_VERSION', line):
      for i in range (len(line.split())):
        if re.match(r'^.*define', line.split()[i]):
          patch = line.split()[i + 2]
  return major, minor, patch


node_version_h = os.path.join(os.path.dirname(__file__),
                              '..',
                              'src',
                              'node_version.h')
with open(node_version_h) as in_file:
  print('.'.join(get_major_minor_patch(in_file.read())))
