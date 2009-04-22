"""SCons.Tool.aixcc

Tool-specific initialization for IBM xlc / Visual Age C compiler.

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

__revision__ = "src/engine/SCons/Tool/aixcc.py 3842 2008/12/20 22:59:52 scons"

import os.path

import SCons.Platform.aix

import cc

packages = ['vac.C', 'ibmcxx.cmp']

def get_xlc(env):
    xlc = env.get('CC', 'xlc')
    xlc_r = env.get('SHCC', 'xlc_r')
    return SCons.Platform.aix.get_xlc(env, xlc, xlc_r, packages)

def generate(env):
    """Add Builders and construction variables for xlc / Visual Age
    suite to an Environment."""
    path, _cc, _shcc, version = get_xlc(env)
    if path:
        _cc = os.path.join(path, _cc)
        _shcc = os.path.join(path, _shcc)

    cc.generate(env)

    env['CC'] = _cc
    env['SHCC'] = _shcc
    env['CCVERSION'] = version

def exists(env):
    path, _cc, _shcc, version = get_xlc(env)
    if path and _cc:
        xlc = os.path.join(path, _cc)
        if os.path.exists(xlc):
            return xlc
    return None
