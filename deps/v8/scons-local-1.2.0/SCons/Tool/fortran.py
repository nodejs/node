"""SCons.Tool.fortran

Tool-specific initialization for a generic Posix f77/f90 Fortran compiler.

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

__revision__ = "src/engine/SCons/Tool/fortran.py 3842 2008/12/20 22:59:52 scons"

import re
import string

import SCons.Action
import SCons.Defaults
import SCons.Scanner.Fortran
import SCons.Tool
import SCons.Util
from SCons.Tool.FortranCommon import add_all_to_env, add_fortran_to_env

compilers = ['f95', 'f90', 'f77']

def generate(env):
    add_all_to_env(env)
    add_fortran_to_env(env)

    fc = env.Detect(compilers) or 'f77'
    env['SHFORTRAN'] = fc
    env['FORTRAN'] = fc

def exists(env):
    return env.Detect(compilers)
