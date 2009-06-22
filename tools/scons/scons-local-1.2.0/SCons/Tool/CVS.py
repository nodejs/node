"""SCons.Tool.CVS.py

Tool-specific initialization for CVS.

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

__revision__ = "src/engine/SCons/Tool/CVS.py 3842 2008/12/20 22:59:52 scons"

import SCons.Action
import SCons.Builder
import SCons.Util

def generate(env):
    """Add a Builder factory function and construction variables for
    CVS to an Environment."""

    def CVSFactory(repos, module='', env=env):
        """ """
        # fail if repos is not an absolute path name?
        if module != '':
           # Don't use os.path.join() because the name we fetch might
           # be across a network and must use POSIX slashes as separators.
           module = module + '/'
           env['CVSCOM']   = '$CVS $CVSFLAGS co $CVSCOFLAGS -d ${TARGET.dir} $CVSMODULE${TARGET.posix}'
        act = SCons.Action.Action('$CVSCOM', '$CVSCOMSTR')
        return SCons.Builder.Builder(action = act,
                                     env = env,
                                     CVSREPOSITORY = repos,
                                     CVSMODULE = module)

    #setattr(env, 'CVS', CVSFactory)
    env.CVS = CVSFactory

    env['CVS']        = 'cvs'
    env['CVSFLAGS']   = SCons.Util.CLVar('-d $CVSREPOSITORY')
    env['CVSCOFLAGS'] = SCons.Util.CLVar('')
    env['CVSCOM']     = '$CVS $CVSFLAGS co $CVSCOFLAGS ${TARGET.posix}'

def exists(env):
    return env.Detect('cvs')
