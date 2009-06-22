"""SCons.Tool.linkloc

Tool specification for the LinkLoc linker for the Phar Lap ETS embedded
operating system.

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

__revision__ = "src/engine/SCons/Tool/linkloc.py 3842 2008/12/20 22:59:52 scons"

import os.path
import re

import SCons.Action
import SCons.Defaults
import SCons.Errors
import SCons.Tool
import SCons.Util

from SCons.Tool.msvc import get_msvc_paths
from SCons.Tool.PharLapCommon import addPharLapPaths

_re_linker_command = re.compile(r'(\s)@\s*([^\s]+)')

def repl_linker_command(m):
    # Replaces any linker command file directives (e.g. "@foo.lnk") with
    # the actual contents of the file.
    try:
        f=open(m.group(2), "r")
        return m.group(1) + f.read()
    except IOError:
        # the linker should return an error if it can't
        # find the linker command file so we will remain quiet.
        # However, we will replace the @ with a # so we will not continue
        # to find it with recursive substitution
        return m.group(1) + '#' + m.group(2)

class LinklocGenerator:
    def __init__(self, cmdline):
        self.cmdline = cmdline

    def __call__(self, env, target, source, for_signature):
        if for_signature:
            # Expand the contents of any linker command files recursively
            subs = 1
            strsub = env.subst(self.cmdline, target=target, source=source)
            while subs:
                strsub, subs = _re_linker_command.subn(repl_linker_command, strsub)
            return strsub
        else:
            return "${TEMPFILE('" + self.cmdline + "')}"

def generate(env):
    """Add Builders and construction variables for ar to an Environment."""
    SCons.Tool.createSharedLibBuilder(env)
    SCons.Tool.createProgBuilder(env)

    env['SUBST_CMD_FILE'] = LinklocGenerator
    env['SHLINK']      = '$LINK'
    env['SHLINKFLAGS'] = SCons.Util.CLVar('$LINKFLAGS')
    env['SHLINKCOM']   = '${SUBST_CMD_FILE("$SHLINK $SHLINKFLAGS $( $_LIBDIRFLAGS $) $_LIBFLAGS -dll $TARGET $SOURCES")}'
    env['SHLIBEMITTER']= None
    env['LINK']        = "linkloc"
    env['LINKFLAGS']   = SCons.Util.CLVar('')
    env['LINKCOM']     = '${SUBST_CMD_FILE("$LINK $LINKFLAGS $( $_LIBDIRFLAGS $) $_LIBFLAGS -exe $TARGET $SOURCES")}'
    env['LIBDIRPREFIX']='-libpath '
    env['LIBDIRSUFFIX']=''
    env['LIBLINKPREFIX']='-lib '
    env['LIBLINKSUFFIX']='$LIBSUFFIX'

    msvs_version = env.get('MSVS_VERSION')
    include_path, lib_path, exe_path = get_msvc_paths(env, version = msvs_version)
    env['ENV']['LIB'] = lib_path
    env.PrependENVPath('PATH', exe_path)

    addPharLapPaths(env)

def exists(env):
    return env.Detect('linkloc')
