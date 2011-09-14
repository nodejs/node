import sys,re; 
for line in sys.stdin:
  if re.match('#define NODE_MAJOR_VERSION', line):
    major = line.split()[2]
  if re.match('#define NODE_MINOR_VERSION', line):
    minor = line.split()[2]
  if re.match('#define NODE_PATCH_VERSION', line):
    patch = line.split()[2]
print '{0:s}.{1:s}.{2:s}.0'.format(major, minor, patch)
