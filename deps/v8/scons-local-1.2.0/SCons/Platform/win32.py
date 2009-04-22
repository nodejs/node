"""SCons.Platform.win32

Platform-specific initialization for Win32 systems.

There normally shouldn't be any need to import this module directly.  It
will usually be imported through the generic SCons.Platform.Platform()
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

__revision__ = "src/engine/SCons/Platform/win32.py 3842 2008/12/20 22:59:52 scons"

import os
import os.path
import string
import sys
import tempfile

from SCons.Platform.posix import exitvalmap
from SCons.Platform import TempFileMunge
import SCons.Util



try:
    import msvcrt
    import win32api
    import win32con

    msvcrt.get_osfhandle
    win32api.SetHandleInformation
    win32con.HANDLE_FLAG_INHERIT
except ImportError:
    parallel_msg = \
        "you do not seem to have the pywin32 extensions installed;\n" + \
        "\tparallel (-j) builds may not work reliably with open Python files."
except AttributeError:
    parallel_msg = \
        "your pywin32 extensions do not support file handle operations;\n" + \
        "\tparallel (-j) builds may not work reliably with open Python files."
else:
    parallel_msg = None

    import __builtin__

    _builtin_file = __builtin__.file
    _builtin_open = __builtin__.open

    def _scons_file(*args, **kw):
        fp = apply(_builtin_file, args, kw)
        win32api.SetHandleInformation(msvcrt.get_osfhandle(fp.fileno()),
                                      win32con.HANDLE_FLAG_INHERIT,
                                      0)
        return fp

    def _scons_open(*args, **kw):
        fp = apply(_builtin_open, args, kw)
        win32api.SetHandleInformation(msvcrt.get_osfhandle(fp.fileno()),
                                      win32con.HANDLE_FLAG_INHERIT,
                                      0)
        return fp

    __builtin__.file = _scons_file
    __builtin__.open = _scons_open



# The upshot of all this is that, if you are using Python 1.5.2,
# you had better have cmd or command.com in your PATH when you run
# scons.

def piped_spawn(sh, escape, cmd, args, env, stdout, stderr):
    # There is no direct way to do that in python. What we do
    # here should work for most cases:
    #   In case stdout (stderr) is not redirected to a file,
    #   we redirect it into a temporary file tmpFileStdout
    #   (tmpFileStderr) and copy the contents of this file
    #   to stdout (stderr) given in the argument
    if not sh:
        sys.stderr.write("scons: Could not find command interpreter, is it in your PATH?\n")
        return 127
    else:
        # one temporary file for stdout and stderr
        tmpFileStdout = os.path.normpath(tempfile.mktemp())
        tmpFileStderr = os.path.normpath(tempfile.mktemp())

        # check if output is redirected
        stdoutRedirected = 0
        stderrRedirected = 0
        for arg in args:
            # are there more possibilities to redirect stdout ?
            if (string.find( arg, ">", 0, 1 ) != -1 or
                string.find( arg, "1>", 0, 2 ) != -1):
                stdoutRedirected = 1
            # are there more possibilities to redirect stderr ?
            if string.find( arg, "2>", 0, 2 ) != -1:
                stderrRedirected = 1

        # redirect output of non-redirected streams to our tempfiles
        if stdoutRedirected == 0:
            args.append(">" + str(tmpFileStdout))
        if stderrRedirected == 0:
            args.append("2>" + str(tmpFileStderr))

        # actually do the spawn
        try:
            args = [sh, '/C', escape(string.join(args)) ]
            ret = os.spawnve(os.P_WAIT, sh, args, env)
        except OSError, e:
            # catch any error
            try:
                ret = exitvalmap[e[0]]
            except KeyError:
                sys.stderr.write("scons: unknown OSError exception code %d - %s: %s\n" % (e[0], cmd, e[1]))
            if stderr != None:
                stderr.write("scons: %s: %s\n" % (cmd, e[1]))
        # copy child output from tempfiles to our streams
        # and do clean up stuff
        if stdout != None and stdoutRedirected == 0:
            try:
                stdout.write(open( tmpFileStdout, "r" ).read())
                os.remove( tmpFileStdout )
            except (IOError, OSError):
                pass

        if stderr != None and stderrRedirected == 0:
            try:
                stderr.write(open( tmpFileStderr, "r" ).read())
                os.remove( tmpFileStderr )
            except (IOError, OSError):
                pass
        return ret

def exec_spawn(l, env):
    try:
        result = os.spawnve(os.P_WAIT, l[0], l, env)
    except OSError, e:
        try:
            result = exitvalmap[e[0]]
            sys.stderr.write("scons: %s: %s\n" % (l[0], e[1]))
        except KeyError:
            result = 127
            if len(l) > 2:
                if len(l[2]) < 1000:
                    command = string.join(l[0:3])
                else:
                    command = l[0]
            else:
                command = l[0]
            sys.stderr.write("scons: unknown OSError exception code %d - '%s': %s\n" % (e[0], command, e[1]))
    return result

def spawn(sh, escape, cmd, args, env):
    if not sh:
        sys.stderr.write("scons: Could not find command interpreter, is it in your PATH?\n")
        return 127
    return exec_spawn([sh, '/C', escape(string.join(args))], env)

# Windows does not allow special characters in file names anyway, so no
# need for a complex escape function, we will just quote the arg, except
# that "cmd /c" requires that if an argument ends with a backslash it
# needs to be escaped so as not to interfere with closing double quote
# that we add.
def escape(x):
    if x[-1] == '\\':
        x = x + '\\'
    return '"' + x + '"'

# Get the windows system directory name
def get_system_root():
    # A resonable default if we can't read the registry
    try:
        val = os.environ['SYSTEMROOT']
    except KeyError:
        val = "C:/WINDOWS"
        pass

    # First see if we can look in the registry...
    if SCons.Util.can_read_reg:
        try:
            # Look for Windows NT system root
            k=SCons.Util.RegOpenKeyEx(SCons.Util.hkey_mod.HKEY_LOCAL_MACHINE,
                                      'Software\\Microsoft\\Windows NT\\CurrentVersion')
            val, tok = SCons.Util.RegQueryValueEx(k, 'SystemRoot')
        except SCons.Util.RegError:
            try:
                # Okay, try the Windows 9x system root
                k=SCons.Util.RegOpenKeyEx(SCons.Util.hkey_mod.HKEY_LOCAL_MACHINE,
                                          'Software\\Microsoft\\Windows\\CurrentVersion')
                val, tok = SCons.Util.RegQueryValueEx(k, 'SystemRoot')
            except KeyboardInterrupt:
                raise
            except:
                pass
    return val

# Get the location of the program files directory
def get_program_files_dir():
    # Now see if we can look in the registry...
    val = ''
    if SCons.Util.can_read_reg:
        try:
            # Look for Windows Program Files directory
            k=SCons.Util.RegOpenKeyEx(SCons.Util.hkey_mod.HKEY_LOCAL_MACHINE,
                                      'Software\\Microsoft\\Windows\\CurrentVersion')
            val, tok = SCons.Util.RegQueryValueEx(k, 'ProgramFilesDir')
        except SCons.Util.RegError:
            val = ''
            pass

    if val == '':
        # A reasonable default if we can't read the registry
        # (Actually, it's pretty reasonable even if we can :-)
        val = os.path.join(os.path.dirname(get_system_root()),"Program Files")
        
    return val

def generate(env):
    # Attempt to find cmd.exe (for WinNT/2k/XP) or
    # command.com for Win9x
    cmd_interp = ''
    # First see if we can look in the registry...
    if SCons.Util.can_read_reg:
        try:
            # Look for Windows NT system root
            k=SCons.Util.RegOpenKeyEx(SCons.Util.hkey_mod.HKEY_LOCAL_MACHINE,
                                          'Software\\Microsoft\\Windows NT\\CurrentVersion')
            val, tok = SCons.Util.RegQueryValueEx(k, 'SystemRoot')
            cmd_interp = os.path.join(val, 'System32\\cmd.exe')
        except SCons.Util.RegError:
            try:
                # Okay, try the Windows 9x system root
                k=SCons.Util.RegOpenKeyEx(SCons.Util.hkey_mod.HKEY_LOCAL_MACHINE,
                                              'Software\\Microsoft\\Windows\\CurrentVersion')
                val, tok = SCons.Util.RegQueryValueEx(k, 'SystemRoot')
                cmd_interp = os.path.join(val, 'command.com')
            except KeyboardInterrupt:
                raise
            except:
                pass

    # For the special case of not having access to the registry, we
    # use a temporary path and pathext to attempt to find the command
    # interpreter.  If we fail, we try to find the interpreter through
    # the env's PATH.  The problem with that is that it might not
    # contain an ENV and a PATH.
    if not cmd_interp:
        systemroot = r'C:\Windows'
        if os.environ.has_key('SYSTEMROOT'):
            systemroot = os.environ['SYSTEMROOT']
        tmp_path = systemroot + os.pathsep + \
                   os.path.join(systemroot,'System32')
        tmp_pathext = '.com;.exe;.bat;.cmd'
        if os.environ.has_key('PATHEXT'):
            tmp_pathext = os.environ['PATHEXT'] 
        cmd_interp = SCons.Util.WhereIs('cmd', tmp_path, tmp_pathext)
        if not cmd_interp:
            cmd_interp = SCons.Util.WhereIs('command', tmp_path, tmp_pathext)

    if not cmd_interp:
        cmd_interp = env.Detect('cmd')
        if not cmd_interp:
            cmd_interp = env.Detect('command')

    
    if not env.has_key('ENV'):
        env['ENV']        = {}

    # Import things from the external environment to the construction
    # environment's ENV.  This is a potential slippery slope, because we
    # *don't* want to make builds dependent on the user's environment by
    # default.  We're doing this for SYSTEMROOT, though, because it's
    # needed for anything that uses sockets, and seldom changes, and
    # for SYSTEMDRIVE because it's related.
    #
    # Weigh the impact carefully before adding other variables to this list.
    import_env = [ 'SYSTEMDRIVE', 'SYSTEMROOT', 'TEMP', 'TMP' ]
    for var in import_env:
        v = os.environ.get(var)
        if v:
            env['ENV'][var] = v

    env['ENV']['PATHEXT'] = '.COM;.EXE;.BAT;.CMD'
    env['OBJPREFIX']      = ''
    env['OBJSUFFIX']      = '.obj'
    env['SHOBJPREFIX']    = '$OBJPREFIX'
    env['SHOBJSUFFIX']    = '$OBJSUFFIX'
    env['PROGPREFIX']     = ''
    env['PROGSUFFIX']     = '.exe'
    env['LIBPREFIX']      = ''
    env['LIBSUFFIX']      = '.lib'
    env['SHLIBPREFIX']    = ''
    env['SHLIBSUFFIX']    = '.dll'
    env['LIBPREFIXES']    = [ '$LIBPREFIX' ]
    env['LIBSUFFIXES']    = [ '$LIBSUFFIX' ]
    env['PSPAWN']         = piped_spawn
    env['SPAWN']          = spawn
    env['SHELL']          = cmd_interp
    env['TEMPFILE']       = TempFileMunge
    env['TEMPFILEPREFIX'] = '@'
    env['MAXLINELENGTH']  = 2048
    env['ESCAPE']         = escape
