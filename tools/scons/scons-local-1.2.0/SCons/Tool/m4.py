"""SCons.Tool.m4

Tool-specific initialization for m4.

There normally shouldn't be any need to import this module directly.
It will usually be imported through the generic SCons.Tool.Tool()
selection method.

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

__revision__ = "src/engine/SCons/Tool/m4.py 3842 2008/12/20 22:59:52 scons"

import SCons.Action
import SCons.Builder
import SCons.Util

def generate(env):
    """Add Builders and construction variables for m4 to an Environment."""
    M4Action = SCons.Action.Action('$M4COM', '$M4COMSTR')
    bld = SCons.Builder.Builder(action = M4Action, src_suffix = '.m4')

    env['BUILDERS']['M4'] = bld

    # .m4 files might include other files, and it would be pretty hard
    # to write a scanner for it, so let's just cd to the dir of the m4
    # file and run from there.
    # The src_suffix setup is like so: file.c.m4 -> file.c,
    # file.cpp.m4 -> file.cpp etc.
    env['M4']      = 'm4'
    env['M4FLAGS'] = SCons.Util.CLVar('-E')
    env['M4COM']   = 'cd ${SOURCE.rsrcdir} && $M4 $M4FLAGS < ${SOURCE.file} > ${TARGET.abspath}'

def exists(env):
    return env.Detect('m4')
