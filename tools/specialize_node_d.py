#!/usr/bin/env python

#
# specialize_node_d.py output_file src/node.d flavor arch
#
# Specialize node.d for given flavor (`freebsd`) and arch (`x64` or `ia32`)
#

import re
import sys

if len(sys.argv) != 5:
  print "usage: specialize_node_d.py outfile src/node.d flavor arch"
  sys.exit(2);

outfile = file(sys.argv[1], 'w');
infile = file(sys.argv[2], 'r');
flavor = sys.argv[3];
arch = sys.argv[4];

model = r'curpsinfo->pr_dmodel == PR_MODEL_ILP32'

for line in infile:
  if flavor == 'freebsd':
    line = re.sub('procfs.d', 'psinfo.d', line);
    if arch == 'x64':
      line = re.sub(model, '0', line);
    else:
      line = re.sub(model, '1', line);
  outfile.write(line);
