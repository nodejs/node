"""SCons.Tool.Perforce.py

Tool-specific initialization for Perforce Source Code Management system.

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

__revision__ = "src/engine/SCons/Tool/Perforce.py 3842 2008/12/20 22:59:52 scons"

import os

import SCons.Action
import SCons.Builder
import SCons.Node.FS
import SCons.Util

# This function should maybe be moved to SCons.Util?
from SCons.Tool.PharLapCommon import addPathIfNotExists



# Variables that we want to import from the base OS environment.
_import_env = [ 'P4PORT', 'P4CLIENT', 'P4USER', 'USER', 'USERNAME', 'P4PASSWD',
                'P4CHARSET', 'P4LANGUAGE', 'SYSTEMROOT' ]

PerforceAction = SCons.Action.Action('$P4COM', '$P4COMSTR')

def generate(env):
    """Add a Builder factory function and construction variables for
    Perforce to an Environment."""

    def PerforceFactory(env=env):
        """ """
        return SCons.Builder.Builder(action = PerforceAction, env = env)

    #setattr(env, 'Perforce', PerforceFactory)
    env.Perforce = PerforceFactory

    env['P4']      = 'p4'
    env['P4FLAGS'] = SCons.Util.CLVar('')
    env['P4COM']   = '$P4 $P4FLAGS sync $TARGET'
    try:
        environ = env['ENV']
    except KeyError:
        environ = {}
        env['ENV'] = environ

    # Perforce seems to use the PWD environment variable rather than
    # calling getcwd() for itself, which is odd.  If no PWD variable
    # is present, p4 WILL call getcwd, but this seems to cause problems
    # with good ol' Windows's tilde-mangling for long file names.
    environ['PWD'] = env.Dir('#').get_abspath()

    for var in _import_env:
        v = os.environ.get(var)
        if v:
            environ[var] = v

    if SCons.Util.can_read_reg:
        # If we can read the registry, add the path to Perforce to our environment.
        try:
            k=SCons.Util.RegOpenKeyEx(SCons.Util.hkey_mod.HKEY_LOCAL_MACHINE,
                                      'Software\\Perforce\\environment')
            val, tok = SCons.Util.RegQueryValueEx(k, 'P4INSTROOT')
            addPathIfNotExists(environ, 'PATH', val)
        except SCons.Util.RegError:
            # Can't detect where Perforce is, hope the user has it set in the
            # PATH.
            pass

def exists(env):
    return env.Detect('p4')
