"""SCons.Tool.aixc++

Tool-specific initialization for IBM xlC / Visual Age C++ compiler.

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

__revision__ = "src/engine/SCons/Tool/aixc++.py 3842 2008/12/20 22:59:52 scons"

import os.path

import SCons.Platform.aix

cplusplus = __import__('c++', globals(), locals(), [])

packages = ['vacpp.cmp.core', 'vacpp.cmp.batch', 'vacpp.cmp.C', 'ibmcxx.cmp']

def get_xlc(env):
    xlc = env.get('CXX', 'xlC')
    xlc_r = env.get('SHCXX', 'xlC_r')
    return SCons.Platform.aix.get_xlc(env, xlc, xlc_r, packages)

def smart_cxxflags(source, target, env, for_signature):
    build_dir = env.GetBuildPath()
    if build_dir:
        return '-qtempinc=' + os.path.join(build_dir, 'tempinc')
    return ''

def generate(env):
    """Add Builders and construction variables for xlC / Visual Age
    suite to an Environment."""
    path, _cxx, _shcxx, version = get_xlc(env)
    if path:
        _cxx = os.path.join(path, _cxx)
        _shcxx = os.path.join(path, _shcxx)

    cplusplus.generate(env)

    env['CXX'] = _cxx
    env['SHCXX'] = _shcxx
    env['CXXVERSION'] = version
    env['SHOBJSUFFIX'] = '.pic.o'
    
def exists(env):
    path, _cxx, _shcxx, version = get_xlc(env)
    if path and _cxx:
        xlc = os.path.join(path, _cxx)
        if os.path.exists(xlc):
            return xlc
    return None
