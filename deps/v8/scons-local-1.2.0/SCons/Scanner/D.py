"""SCons.Scanner.D

Scanner for the Digital Mars "D" programming language.

Coded by Andy Friesen
17 Nov 2003

"""

#
# Copyright (c) 2001, 2002, 2003, 2004, 2005, 2006, 2007, 2008 The SCons Foundation
#
# Permission is hereby granted, free of charge, to any person obtaining
# a copy of this software and associated documentation files (the
# "Software"), to deal in the Software without restriction, including
# without limitation the rights to use, copy, modify, merge, publish,
# distribute, sublicense, and/or sell copies of the Software, and to
# permit persons to whom the Software is furnished to do so, subject to
# the following conditions:
#
# The above copyright notice and this permission notice shall be included
# in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY
# KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
# WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
# NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
# LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
# OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
# WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
#

__revision__ = "src/engine/SCons/Scanner/D.py 3842 2008/12/20 22:59:52 scons"

import re
import string

import SCons.Scanner

def DScanner():
    """Return a prototype Scanner instance for scanning D source files"""
    ds = D()
    return ds

class D(SCons.Scanner.Classic):
    def __init__ (self):
        SCons.Scanner.Classic.__init__ (self,
            name = "DScanner",
            suffixes = '$DSUFFIXES',
            path_variable = 'DPATH',
            regex = 'import\s+(?:[a-zA-Z0-9_.]+)\s*(?:,\s*(?:[a-zA-Z0-9_.]+)\s*)*;')

        self.cre2 = re.compile ('(?:import\s)?\s*([a-zA-Z0-9_.]+)\s*(?:,|;)', re.M)

    def find_include(self, include, source_dir, path):
        # translate dots (package separators) to slashes
        inc = string.replace(include, '.', '/')

        i = SCons.Node.FS.find_file(inc + '.d', (source_dir,) + path)
        if i is None:
            i = SCons.Node.FS.find_file (inc + '.di', (source_dir,) + path)
        return i, include

    def find_include_names(self, node):
        includes = []
        for i in self.cre.findall(node.get_contents()):
            includes = includes + self.cre2.findall(i)
        return includes
