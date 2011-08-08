"""SCons.Tool.aixlink

Tool-specific initialization for the IBM Visual Age linker.

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

__revision__ = "src/engine/SCons/Tool/aixlink.py 3842 2008/12/20 22:59:52 scons"

import os
import os.path

import SCons.Util

import aixcc
import link

cplusplus = __import__('c++', globals(), locals(), [])

def smart_linkflags(source, target, env, for_signature):
    if cplusplus.iscplusplus(source):
        build_dir = env.subst('$BUILDDIR', target=target, source=source)
        if build_dir:
            return '-qtempinc=' + os.path.join(build_dir, 'tempinc')
    return ''

def generate(env):
    """
    Add Builders and construction variables for Visual Age linker to
    an Environment.
    """
    link.generate(env)

    env['SMARTLINKFLAGS'] = smart_linkflags
    env['LINKFLAGS']      = SCons.Util.CLVar('$SMARTLINKFLAGS')
    env['SHLINKFLAGS']    = SCons.Util.CLVar('$LINKFLAGS -qmkshrobj -qsuppress=1501-218')
    env['SHLIBSUFFIX']    = '.a'

def exists(env):
    path, _cc, _shcc, version = aixcc.get_xlc(env)
    if path and _cc:
        xlc = os.path.join(path, _cc)
        if os.path.exists(xlc):
            return xlc
    return None
