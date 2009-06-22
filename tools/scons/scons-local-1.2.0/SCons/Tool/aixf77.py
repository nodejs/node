"""engine.SCons.Tool.aixf77

Tool-specific initialization for IBM Visual Age f77 Fortran compiler.

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

__revision__ = "src/engine/SCons/Tool/aixf77.py 3842 2008/12/20 22:59:52 scons"

import os.path

#import SCons.Platform.aix

import f77

# It would be good to look for the AIX F77 package the same way we're now
# looking for the C and C++ packages.  This should be as easy as supplying
# the correct package names in the following list and uncommenting the
# SCons.Platform.aix_get_xlc() call the in the function below.
packages = []

def get_xlf77(env):
    xlf77 = env.get('F77', 'xlf77')
    xlf77_r = env.get('SHF77', 'xlf77_r')
    #return SCons.Platform.aix.get_xlc(env, xlf77, xlf77_r, packages)
    return (None, xlf77, xlf77_r, None)

def generate(env):
    """
    Add Builders and construction variables for the Visual Age FORTRAN
    compiler to an Environment.
    """
    path, _f77, _shf77, version = get_xlf77(env)
    if path:
        _f77 = os.path.join(path, _f77)
        _shf77 = os.path.join(path, _shf77)

    f77.generate(env)

    env['F77'] = _f77
    env['SHF77'] = _shf77

def exists(env):
    path, _f77, _shf77, version = get_xlf77(env)
    if path and _f77:
        xlf77 = os.path.join(path, _f77)
        if os.path.exists(xlf77):
            return xlf77
    return None
