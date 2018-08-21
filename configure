#!/bin/sh

# Locate python2 interpreter and re-execute the script.  Note that the
# mix of single and double quotes is intentional, as is the fact that
# the ] goes on a new line.
_=[ 'exec' '/bin/sh' '-c' '''
which python2.7 >/dev/null && exec python2.7 "$0" "$@"
which python2 >/dev/null && exec python2 "$0" "$@"
exec python "$0" "$@"
''' "$0" "$@"
]
del _

import sys
from distutils.spawn import find_executable as which
if sys.version_info[0] != 2 or sys.version_info[1] not in (6, 7):
  sys.stderr.write('Please use either Python 2.6 or 2.7')

  python2 = which('python2') or which('python2.6') or which('python2.7')

  if python2:
    sys.stderr.write(':\n\n')
    sys.stderr.write('  ' + python2 + ' ' + ' '.join(sys.argv))

  sys.stderr.write('\n')
  sys.exit(1)

import configure
