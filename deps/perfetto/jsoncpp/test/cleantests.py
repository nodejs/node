# Copyright 2007 Baptiste Lepilleur and The JsonCpp Authors
# Distributed under MIT license, or public domain if desired and
# recognized in your jurisdiction.
# See file LICENSE for detail or copy at http://jsoncpp.sourceforge.net/LICENSE

"""Removes all files created during testing."""

import glob
import os

paths = []
for pattern in [ '*.actual', '*.actual-rewrite', '*.rewrite', '*.process-output' ]:
    paths += glob.glob('data/' + pattern)

for path in paths:
    os.unlink(path)
