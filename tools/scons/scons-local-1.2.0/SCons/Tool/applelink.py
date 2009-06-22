"""SCons.Tool.applelink

Tool-specific initialization for the Apple gnu-like linker.

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

__revision__ = "src/engine/SCons/Tool/applelink.py 3842 2008/12/20 22:59:52 scons"

import SCons.Util

# Even though the Mac is based on the GNU toolchain, it doesn't understand
# the -rpath option, so we use the "link" tool instead of "gnulink".
import link

def generate(env):
    """Add Builders and construction variables for applelink to an
    Environment."""
    link.generate(env)

    env['FRAMEWORKPATHPREFIX'] = '-F'
    env['_FRAMEWORKPATH'] = '${_concat(FRAMEWORKPATHPREFIX, FRAMEWORKPATH, "", __env__)}'
    env['_FRAMEWORKS'] = '${_concat("-framework ", FRAMEWORKS, "", __env__)}'
    env['LINKCOM'] = env['LINKCOM'] + ' $_FRAMEWORKPATH $_FRAMEWORKS $FRAMEWORKSFLAGS'
    env['SHLINKFLAGS'] = SCons.Util.CLVar('$LINKFLAGS -dynamiclib')
    env['SHLINKCOM'] = env['SHLINKCOM'] + ' $_FRAMEWORKPATH $_FRAMEWORKS $FRAMEWORKSFLAGS'

    # override the default for loadable modules, which are different
    # on OS X than dynamic shared libs.  echoing what XCode does for
    # pre/suffixes:
    env['LDMODULEPREFIX'] = '' 
    env['LDMODULESUFFIX'] = '' 
    env['LDMODULEFLAGS'] = SCons.Util.CLVar('$LINKFLAGS -bundle')
    env['LDMODULECOM'] = '$LDMODULE -o ${TARGET} $LDMODULEFLAGS $SOURCES $_LIBDIRFLAGS $_LIBFLAGS $_FRAMEWORKPATH $_FRAMEWORKS $FRAMEWORKSFLAGS'



def exists(env):
    return env['PLATFORM'] == 'darwin'
