import sys,os,re

node_version_h = os.path.join(os.path.dirname(__file__), '..', 'src',
    'node_version.h')

f = open(node_version_h)

for line in f:
  if re.match('#define NODE_VERSION_IS_RELEASE', line):
    release = int(line.split()[2])
    print release
