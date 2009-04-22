"""SCons.Tool.dmd

Tool-specific initialization for the Digital Mars D compiler.
(http://digitalmars.com/d)

Coded by Andy Friesen (andy@ikagames.com)
15 November 2003

There are a number of problems with this script at this point in time.
The one that irritates me the most is the Windows linker setup.  The D
linker doesn't have a way to add lib paths on the commandline, as far
as I can see.  You have to specify paths relative to the SConscript or
use absolute paths.  To hack around it, add '#/blah'.  This will link
blah.lib from the directory where SConstruct resides.

Compiler variables:
    DC - The name of the D compiler to use.  Defaults to dmd or gdmd,
    whichever is found.
    DPATH - List of paths to search for import modules.
    DVERSIONS - List of version tags to enable when compiling.
    DDEBUG - List of debug tags to enable when compiling.

Linker related variables:
    LIBS - List of library files to link in.
    DLINK - Name of the linker to use.  Defaults to dmd or gdmd.
    DLINKFLAGS - List of linker flags.

Lib tool variables:
    DLIB - Name of the lib tool to use.  Defaults to lib.
    DLIBFLAGS - List of flags to pass to the lib tool.
    LIBS - Same as for the linker. (libraries to pull into the .lib)
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

__revision__ = "src/engine/SCons/Tool/dmd.py 3842 2008/12/20 22:59:52 scons"

import os
import string

import SCons.Action
import SCons.Builder
import SCons.Defaults
import SCons.Scanner.D
import SCons.Tool

# Adapted from c++.py
def isD(source):
    if not source:
        return 0

    for s in source:
        if s.sources:
            ext = os.path.splitext(str(s.sources[0]))[1]
            if ext == '.d':
                return 1
    return 0

smart_link = {}

smart_lib = {}

def generate(env):
    global smart_link
    global smart_lib

    static_obj, shared_obj = SCons.Tool.createObjBuilders(env)

    DAction = SCons.Action.Action('$DCOM', '$DCOMSTR')

    static_obj.add_action('.d', DAction)
    shared_obj.add_action('.d', DAction)
    static_obj.add_emitter('.d', SCons.Defaults.StaticObjectEmitter)
    shared_obj.add_emitter('.d', SCons.Defaults.SharedObjectEmitter)

    dc = env.Detect(['dmd', 'gdmd'])
    env['DC'] = dc
    env['DCOM'] = '$DC $_DINCFLAGS $_DVERFLAGS $_DDEBUGFLAGS $_DFLAGS -c -of$TARGET $SOURCES'
    env['_DINCFLAGS'] = '$( ${_concat(DINCPREFIX, DPATH, DINCSUFFIX, __env__, RDirs, TARGET, SOURCE)}  $)'
    env['_DVERFLAGS'] = '$( ${_concat(DVERPREFIX, DVERSIONS, DVERSUFFIX, __env__)}  $)'
    env['_DDEBUGFLAGS'] = '$( ${_concat(DDEBUGPREFIX, DDEBUG, DDEBUGSUFFIX, __env__)} $)'
    env['_DFLAGS'] = '$( ${_concat(DFLAGPREFIX, DFLAGS, DFLAGSUFFIX, __env__)} $)'

    env['DPATH'] = ['#/']
    env['DFLAGS'] = []
    env['DVERSIONS'] = []
    env['DDEBUG'] = []

    if dc:
        # Add the path to the standard library.
        # This is merely for the convenience of the dependency scanner.
        dmd_path = env.WhereIs(dc)
        if dmd_path:
            x = string.rindex(dmd_path, dc)
            phobosDir = dmd_path[:x] + '/../src/phobos'
            if os.path.isdir(phobosDir):
                env.Append(DPATH = [phobosDir])

    env['DINCPREFIX'] = '-I'
    env['DINCSUFFIX'] = ''
    env['DVERPREFIX'] = '-version='
    env['DVERSUFFIX'] = ''
    env['DDEBUGPREFIX'] = '-debug='
    env['DDEBUGSUFFIX'] = ''
    env['DFLAGPREFIX'] = '-'
    env['DFLAGSUFFIX'] = ''
    env['DFILESUFFIX'] = '.d'

    # Need to use the Digital Mars linker/lib on windows.
    # *nix can just use GNU link.
    if env['PLATFORM'] == 'win32':
        env['DLINK'] = '$DC'
        env['DLINKCOM'] = '$DLINK -of$TARGET $SOURCES $DFLAGS $DLINKFLAGS $_DLINKLIBFLAGS'
        env['DLIB'] = 'lib'
        env['DLIBCOM'] = '$DLIB $_DLIBFLAGS -c $TARGET $SOURCES $_DLINKLIBFLAGS'

        env['_DLINKLIBFLAGS'] = '$( ${_concat(DLIBLINKPREFIX, LIBS, DLIBLINKSUFFIX, __env__, RDirs, TARGET, SOURCE)} $)'
        env['_DLIBFLAGS'] = '$( ${_concat(DLIBFLAGPREFIX, DLIBFLAGS, DLIBFLAGSUFFIX, __env__)} $)'
        env['DLINKFLAGS'] = []
        env['DLIBLINKPREFIX'] = ''
        env['DLIBLINKSUFFIX'] = '.lib'
        env['DLIBFLAGPREFIX'] = '-'
        env['DLIBFLAGSUFFIX'] = ''
        env['DLINKFLAGPREFIX'] = '-'
        env['DLINKFLAGSUFFIX'] = ''

        SCons.Tool.createStaticLibBuilder(env)

        # Basically, we hijack the link and ar builders with our own.
        # these builders check for the presence of D source, and swap out
        # the system's defaults for the Digital Mars tools.  If there's no D
        # source, then we silently return the previous settings.
        linkcom = env.get('LINKCOM')
        try:
            env['SMART_LINKCOM'] = smart_link[linkcom]
        except KeyError:
            def _smartLink(source, target, env, for_signature,
                           defaultLinker=linkcom):
                if isD(source):
                    # XXX I'm not sure how to add a $DLINKCOMSTR variable
                    # so that it works with this _smartLink() logic,
                    # and I don't have a D compiler/linker to try it out,
                    # so we'll leave it alone for now.
                    return '$DLINKCOM'
                else:
                    return defaultLinker
            env['SMART_LINKCOM'] = smart_link[linkcom] = _smartLink

        arcom = env.get('ARCOM')
        try:
            env['SMART_ARCOM'] = smart_lib[arcom]
        except KeyError:
            def _smartLib(source, target, env, for_signature,
                         defaultLib=arcom):
                if isD(source):
                    # XXX I'm not sure how to add a $DLIBCOMSTR variable
                    # so that it works with this _smartLib() logic, and
                    # I don't have a D compiler/archiver to try it out,
                    # so we'll leave it alone for now.
                    return '$DLIBCOM'
                else:
                    return defaultLib
            env['SMART_ARCOM'] = smart_lib[arcom] = _smartLib

        # It is worth noting that the final space in these strings is
        # absolutely pivotal.  SCons sees these as actions and not generators
        # if it is not there. (very bad)
        env['ARCOM'] = '$SMART_ARCOM '
        env['LINKCOM'] = '$SMART_LINKCOM '
    else: # assuming linux
        linkcom = env.get('LINKCOM')
        try:
            env['SMART_LINKCOM'] = smart_link[linkcom]
        except KeyError:
            def _smartLink(source, target, env, for_signature,
                           defaultLinker=linkcom, dc=dc):
                if isD(source):
                    try:
                        libs = env['LIBS']
                    except KeyError:
                        libs = []
                    if 'phobos' not in libs:
                        if dc is 'dmd':
                            env.Append(LIBS = ['phobos'])
                        elif dc is 'gdmd':
                            env.Append(LIBS = ['gphobos'])
                    if 'pthread' not in libs:
                        env.Append(LIBS = ['pthread'])
                    if 'm' not in libs:
                        env.Append(LIBS = ['m'])
                return defaultLinker
            env['SMART_LINKCOM'] = smart_link[linkcom] = _smartLink

        env['LINKCOM'] = '$SMART_LINKCOM '

def exists(env):
    return env.Detect(['dmd', 'gdmd'])
