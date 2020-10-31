# Copyright 2007 Baptiste Lepilleur and The JsonCpp Authors
# Distributed under MIT license, or public domain if desired and
# recognized in your jurisdiction.
# See file LICENSE for detail or copy at http://jsoncpp.sourceforge.net/LICENSE

from __future__ import print_function
import glob
import os.path
for path in glob.glob('*.json'):
    text = file(path,'rt').read()
    target = os.path.splitext(path)[0] + '.expected'
    if os.path.exists(target):
        print('skipping:', target)
    else:
        print('creating:', target)
        file(target,'wt').write(text)

