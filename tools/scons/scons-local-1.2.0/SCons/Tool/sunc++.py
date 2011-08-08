"""SCons.Tool.sunc++

Tool-specific initialization for C++ on SunOS / Solaris.

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

__revision__ = "src/engine/SCons/Tool/sunc++.py 3842 2008/12/20 22:59:52 scons"

import SCons

import os.path

cplusplus = __import__('c++', globals(), locals(), [])

# use the package installer tool lslpp to figure out where cppc and what
# version of it is installed
def get_cppc(env):
    cxx = env.get('CXX', None)
    if cxx:
        cppcPath = os.path.dirname(cxx)
    else:
        cppcPath = None

    cppcVersion = None

    pkginfo = env.subst('$PKGINFO')
    pkgchk = env.subst('$PKGCHK')

    def look_pkg_db(pkginfo=pkginfo, pkgchk=pkgchk):
        version = None
        path = None
        for package in ['SPROcpl']:
            cmd = "%s -l %s 2>/dev/null | grep '^ *VERSION:'" % (pkginfo, package)
            line = os.popen(cmd).readline()
            if line:
                version = line.split()[-1]
                cmd = "%s -l %s 2>/dev/null | grep '^Pathname:.*/bin/CC$' | grep -v '/SC[0-9]*\.[0-9]*/'" % (pkgchk, package)
                line = os.popen(cmd).readline()
                if line:
                    path = os.path.dirname(line.split()[-1])
                    break

        return path, version

    path, version = look_pkg_db()
    if path and version:
        cppcPath, cppcVersion = path, version

    return (cppcPath, 'CC', 'CC', cppcVersion)

def generate(env):
    """Add Builders and construction variables for SunPRO C++."""
    path, cxx, shcxx, version = get_cppc(env)
    if path:
        cxx = os.path.join(path, cxx)
        shcxx = os.path.join(path, shcxx)

    cplusplus.generate(env)

    env['CXX'] = cxx
    env['SHCXX'] = shcxx
    env['CXXVERSION'] = version
    env['SHCXXFLAGS']   = SCons.Util.CLVar('$CXXFLAGS -KPIC')
    env['SHOBJPREFIX']  = 'so_'
    env['SHOBJSUFFIX']  = '.o'
    
def exists(env):
    path, cxx, shcxx, version = get_cppc(env)
    if path and cxx:
        cppc = os.path.join(path, cxx)
        if os.path.exists(cppc):
            return cppc
    return None
