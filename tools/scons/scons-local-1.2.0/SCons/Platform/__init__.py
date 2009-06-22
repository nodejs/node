"""SCons.Platform

SCons platform selection.

This looks for modules that define a callable object that can modify a
construction environment as appropriate for a given platform.

Note that we take a more simplistic view of "platform" than Python does.
We're looking for a single string that determines a set of
tool-independent variables with which to initialize a construction
environment.  Consequently, we'll examine both sys.platform and os.name
(and anything else that might come in to play) in order to return some
specification which is unique enough for our purposes.

Note that because this subsysem just *selects* a callable that can
modify a construction environment, it's possible for people to define
their own "platform specification" in an arbitrary callable function.
No one needs to use or tie in to this subsystem in order to roll
their own platform definition.
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

__revision__ = "src/engine/SCons/Platform/__init__.py 3842 2008/12/20 22:59:52 scons"

import imp
import os
import string
import sys
import tempfile

import SCons.Errors
import SCons.Tool

def platform_default():
    """Return the platform string for our execution environment.

    The returned value should map to one of the SCons/Platform/*.py
    files.  Since we're architecture independent, though, we don't
    care about the machine architecture.
    """
    osname = os.name
    if osname == 'java':
        osname = os._osType
    if osname == 'posix':
        if sys.platform == 'cygwin':
            return 'cygwin'
        elif string.find(sys.platform, 'irix') != -1:
            return 'irix'
        elif string.find(sys.platform, 'sunos') != -1:
            return 'sunos'
        elif string.find(sys.platform, 'hp-ux') != -1:
            return 'hpux'
        elif string.find(sys.platform, 'aix') != -1:
            return 'aix'
        elif string.find(sys.platform, 'darwin') != -1:
            return 'darwin'
        else:
            return 'posix'
    elif os.name == 'os2':
        return 'os2'
    else:
        return sys.platform

def platform_module(name = platform_default()):
    """Return the imported module for the platform.

    This looks for a module name that matches the specified argument.
    If the name is unspecified, we fetch the appropriate default for
    our execution environment.
    """
    full_name = 'SCons.Platform.' + name
    if not sys.modules.has_key(full_name):
        if os.name == 'java':
            eval(full_name)
        else:
            try:
                file, path, desc = imp.find_module(name,
                                        sys.modules['SCons.Platform'].__path__)
                try:
                    mod = imp.load_module(full_name, file, path, desc)
                finally:
                    if file:
                        file.close()
            except ImportError:
                try:
                    import zipimport
                    importer = zipimport.zipimporter( sys.modules['SCons.Platform'].__path__[0] )
                    mod = importer.load_module(full_name)
                except ImportError:
                    raise SCons.Errors.UserError, "No platform named '%s'" % name
            setattr(SCons.Platform, name, mod)
    return sys.modules[full_name]

def DefaultToolList(platform, env):
    """Select a default tool list for the specified platform.
    """
    return SCons.Tool.tool_list(platform, env)

class PlatformSpec:
    def __init__(self, name):
        self.name = name

    def __str__(self):
        return self.name
        
class TempFileMunge:
    """A callable class.  You can set an Environment variable to this,
    then call it with a string argument, then it will perform temporary
    file substitution on it.  This is used to circumvent the long command
    line limitation.

    Example usage:
    env["TEMPFILE"] = TempFileMunge
    env["LINKCOM"] = "${TEMPFILE('$LINK $TARGET $SOURCES')}"

    By default, the name of the temporary file used begins with a
    prefix of '@'.  This may be configred for other tool chains by
    setting '$TEMPFILEPREFIX'.

    env["TEMPFILEPREFIX"] = '-@'        # diab compiler
    env["TEMPFILEPREFIX"] = '-via'      # arm tool chain
    """
    def __init__(self, cmd):
        self.cmd = cmd

    def __call__(self, target, source, env, for_signature):
        if for_signature:
            return self.cmd
        cmd = env.subst_list(self.cmd, 0, target, source)[0]
        try:
            maxline = int(env.subst('$MAXLINELENGTH'))
        except ValueError:
            maxline = 2048

        if (reduce(lambda x, y: x + len(y), cmd, 0) + len(cmd)) <= maxline:
            return self.cmd

        # We do a normpath because mktemp() has what appears to be
        # a bug in Windows that will use a forward slash as a path
        # delimiter.  Windows's link mistakes that for a command line
        # switch and barfs.
        #
        # We use the .lnk suffix for the benefit of the Phar Lap
        # linkloc linker, which likes to append an .lnk suffix if
        # none is given.
        tmp = os.path.normpath(tempfile.mktemp('.lnk'))
        native_tmp = SCons.Util.get_native_path(tmp)

        if env['SHELL'] and env['SHELL'] == 'sh':
            # The sh shell will try to escape the backslashes in the
            # path, so unescape them.
            native_tmp = string.replace(native_tmp, '\\', r'\\\\')
            # In Cygwin, we want to use rm to delete the temporary
            # file, because del does not exist in the sh shell.
            rm = env.Detect('rm') or 'del'
        else:
            # Don't use 'rm' if the shell is not sh, because rm won't
            # work with the Windows shells (cmd.exe or command.com) or
            # Windows path names.
            rm = 'del'

        prefix = env.subst('$TEMPFILEPREFIX')
        if not prefix:
            prefix = '@'

        args = map(SCons.Subst.quote_spaces, cmd[1:])
        open(tmp, 'w').write(string.join(args, " ") + "\n")
        # XXX Using the SCons.Action.print_actions value directly
        # like this is bogus, but expedient.  This class should
        # really be rewritten as an Action that defines the
        # __call__() and strfunction() methods and lets the
        # normal action-execution logic handle whether or not to
        # print/execute the action.  The problem, though, is all
        # of that is decided before we execute this method as
        # part of expanding the $TEMPFILE construction variable.
        # Consequently, refactoring this will have to wait until
        # we get more flexible with allowing Actions to exist
        # independently and get strung together arbitrarily like
        # Ant tasks.  In the meantime, it's going to be more
        # user-friendly to not let obsession with architectural
        # purity get in the way of just being helpful, so we'll
        # reach into SCons.Action directly.
        if SCons.Action.print_actions:
            print("Using tempfile "+native_tmp+" for command line:\n"+
                  str(cmd[0]) + " " + string.join(args," "))
        return [ cmd[0], prefix + native_tmp + '\n' + rm, native_tmp ]
    
def Platform(name = platform_default()):
    """Select a canned Platform specification.
    """
    module = platform_module(name)
    spec = PlatformSpec(name)
    spec.__call__ = module.generate
    return spec
