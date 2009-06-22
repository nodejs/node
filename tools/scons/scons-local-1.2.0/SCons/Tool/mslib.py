"""SCons.Tool.mslib

Tool-specific initialization for lib (MicroSoft library archiver).

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

__revision__ = "src/engine/SCons/Tool/mslib.py 3842 2008/12/20 22:59:52 scons"

import SCons.Defaults
import SCons.Tool
import SCons.Tool.msvs
import SCons.Tool.msvc
import SCons.Util

def generate(env):
    """Add Builders and construction variables for lib to an Environment."""
    SCons.Tool.createStaticLibBuilder(env)

    try:
        version = SCons.Tool.msvs.get_default_visualstudio_version(env)

        if env.has_key('MSVS_IGNORE_IDE_PATHS') and env['MSVS_IGNORE_IDE_PATHS']:
            include_path, lib_path, exe_path = SCons.Tool.msvc.get_msvc_default_paths(env,version)
        else:
            include_path, lib_path, exe_path = SCons.Tool.msvc.get_msvc_paths(env,version)

        # since other tools can set this, we just make sure that the
        # relevant stuff from MSVS is in there somewhere.
        env.PrependENVPath('PATH', exe_path)
    except (SCons.Util.RegError, SCons.Errors.InternalError):
        pass

    env['AR']          = 'lib'
    env['ARFLAGS']     = SCons.Util.CLVar('/nologo')
    env['ARCOM']       = "${TEMPFILE('$AR $ARFLAGS /OUT:$TARGET $SOURCES')}"
    env['LIBPREFIX']   = ''
    env['LIBSUFFIX']   = '.lib'

def exists(env):
    try:
        v = SCons.Tool.msvs.get_visualstudio_versions()
    except (SCons.Util.RegError, SCons.Errors.InternalError):
        pass

    if not v:
        return env.Detect('lib')
    else:
        # there's at least one version of MSVS installed.
        return 1
