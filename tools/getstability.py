import os, re, sys

node_version_h = os.path.join(os.path.dirname(__file__), '..', 'src',
    'node_version.h')

f = open(node_version_h)

for line in f:
  if re.match('#define NODE_MAJOR_VERSION', line):
    major = line.split()[2]
  if re.match('#define NODE_MINOR_VERSION', line):
    minor = line.split()[2]
  if re.match('#define NODE_PATCH_VERSION', line):
    patch = line.split()[2]

major_minor = major + '.' + minor
if major_minor == '0.10':
  print 'maintenance'
elif major_minor == '0.12':
  print 'stable'
elif minor % 2 != 0:
  print 'unstable'
else:
  print 'Unknown stability status, exiting'
  sys.exit(1)
