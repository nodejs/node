"""SCons.Tool.dvi

Common DVI Builder definition for various other Tool modules that use it.

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

__revision__ = "src/engine/SCons/Tool/dvi.py 3842 2008/12/20 22:59:52 scons"

import SCons.Builder
import SCons.Tool

DVIBuilder = None

def generate(env):
    try:
        env['BUILDERS']['DVI']
    except KeyError:
        global DVIBuilder

        if DVIBuilder is None:
            # The suffix is hard-coded to '.dvi', not configurable via a
            # construction variable like $DVISUFFIX, because the output
            # file name is hard-coded within TeX.
            DVIBuilder = SCons.Builder.Builder(action = {},
                                               source_scanner = SCons.Tool.LaTeXScanner,
                                               suffix = '.dvi',
                                               emitter = {},
                                               source_ext_match = None)

        env['BUILDERS']['DVI'] = DVIBuilder

def exists(env):
    # This only puts a skeleton Builder in place, so if someone
    # references this Tool directly, it's always "available."
    return 1
