#!/usr/bin/env python

import sys

open(sys.argv[1], 'w').write(open(sys.argv[2]).read())
