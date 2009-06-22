"""SCons.Tool.yacc

Tool-specific initialization for yacc.

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

__revision__ = "src/engine/SCons/Tool/yacc.py 3842 2008/12/20 22:59:52 scons"

import os.path
import string

import SCons.Defaults
import SCons.Tool
import SCons.Util

YaccAction = SCons.Action.Action("$YACCCOM", "$YACCCOMSTR")

def _yaccEmitter(target, source, env, ysuf, hsuf):
    yaccflags = env.subst("$YACCFLAGS", target=target, source=source)
    flags = SCons.Util.CLVar(yaccflags)
    targetBase, targetExt = os.path.splitext(SCons.Util.to_String(target[0]))

    if '.ym' in ysuf:                # If using Objective-C
        target = [targetBase + ".m"] # the extension is ".m".


    # If -d is specified on the command line, yacc will emit a .h
    # or .hpp file with the same name as the .c or .cpp output file.
    if '-d' in flags:
        target.append(targetBase + env.subst(hsuf, target=target, source=source))

    # If -g is specified on the command line, yacc will emit a .vcg
    # file with the same base name as the .y, .yacc, .ym or .yy file.
    if "-g" in flags:
        base, ext = os.path.splitext(SCons.Util.to_String(source[0]))
        target.append(base + env.subst("$YACCVCGFILESUFFIX"))

    # With --defines and --graph, the name of the file is totally defined
    # in the options.
    fileGenOptions = ["--defines=", "--graph="]
    for option in flags:
        for fileGenOption in fileGenOptions:
            l = len(fileGenOption)
            if option[:l] == fileGenOption:
                # A file generating option is present, so add the file
                # name to the list of targets.
                fileName = string.strip(option[l:])
                target.append(fileName)

    return (target, source)

def yEmitter(target, source, env):
    return _yaccEmitter(target, source, env, ['.y', '.yacc'], '$YACCHFILESUFFIX')

def ymEmitter(target, source, env):
    return _yaccEmitter(target, source, env, ['.ym'], '$YACCHFILESUFFIX')

def yyEmitter(target, source, env):
    return _yaccEmitter(target, source, env, ['.yy'], '$YACCHXXFILESUFFIX')

def generate(env):
    """Add Builders and construction variables for yacc to an Environment."""
    c_file, cxx_file = SCons.Tool.createCFileBuilders(env)

    # C
    c_file.add_action('.y', YaccAction)
    c_file.add_emitter('.y', yEmitter)

    c_file.add_action('.yacc', YaccAction)
    c_file.add_emitter('.yacc', yEmitter)

    # Objective-C
    c_file.add_action('.ym', YaccAction)
    c_file.add_emitter('.ym', ymEmitter)

    # C++
    cxx_file.add_action('.yy', YaccAction)
    cxx_file.add_emitter('.yy', yyEmitter)

    env['YACC']      = env.Detect('bison') or 'yacc'
    env['YACCFLAGS'] = SCons.Util.CLVar('')
    env['YACCCOM']   = '$YACC $YACCFLAGS -o $TARGET $SOURCES'
    env['YACCHFILESUFFIX'] = '.h'

    # Apparently, OS X now creates file.hpp like everybody else
    # I have no idea when it changed; it was fixed in 10.4
    #if env['PLATFORM'] == 'darwin':
    #    # Bison on Mac OS X just appends ".h" to the generated target .cc
    #    # or .cpp file name.  Hooray for delayed expansion of variables.
    #    env['YACCHXXFILESUFFIX'] = '${TARGET.suffix}.h'
    #else:
    #    env['YACCHXXFILESUFFIX'] = '.hpp'
    env['YACCHXXFILESUFFIX'] = '.hpp'

    env['YACCVCGFILESUFFIX'] = '.vcg'

def exists(env):
    return env.Detect(['bison', 'yacc'])
