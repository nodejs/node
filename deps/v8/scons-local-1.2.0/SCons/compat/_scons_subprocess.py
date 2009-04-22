# subprocess - Subprocesses with accessible I/O streams
#
# For more information about this module, see PEP 324.
#
# This module should remain compatible with Python 2.2, see PEP 291.
#
# Copyright (c) 2003-2005 by Peter Astrand <astrand@lysator.liu.se>
#
# Licensed to PSF under a Contributor Agreement.
# See http://www.python.org/2.4/license for licensing details.

r"""subprocess - Subprocesses with accessible I/O streams

This module allows you to spawn processes, connect to their
input/output/error pipes, and obtain their return codes.  This module
intends to replace several other, older modules and functions, like:

os.system
os.spawn*
os.popen*
popen2.*
commands.*

Information about how the subprocess module can be used to replace these
modules and functions can be found below.



Using the subprocess module
===========================
This module defines one class called Popen:

class Popen(args, bufsize=0, executable=None,
            stdin=None, stdout=None, stderr=None,
            preexec_fn=None, close_fds=False, shell=False,
            cwd=None, env=None, universal_newlines=False,
            startupinfo=None, creationflags=0):


Arguments are:

args should be a string, or a sequence of program arguments.  The
program to execute is normally the first item in the args sequence or
string, but can be explicitly set by using the executable argument.

On UNIX, with shell=False (default): In this case, the Popen class
uses os.execvp() to execute the child program.  args should normally
be a sequence.  A string will be treated as a sequence with the string
as the only item (the program to execute).

On UNIX, with shell=True: If args is a string, it specifies the
command string to execute through the shell.  If args is a sequence,
the first item specifies the command string, and any additional items
will be treated as additional shell arguments.

On Windows: the Popen class uses CreateProcess() to execute the child
program, which operates on strings.  If args is a sequence, it will be
converted to a string using the list2cmdline method.  Please note that
not all MS Windows applications interpret the command line the same
way: The list2cmdline is designed for applications using the same
rules as the MS C runtime.

bufsize, if given, has the same meaning as the corresponding argument
to the built-in open() function: 0 means unbuffered, 1 means line
buffered, any other positive value means use a buffer of
(approximately) that size.  A negative bufsize means to use the system
default, which usually means fully buffered.  The default value for
bufsize is 0 (unbuffered).

stdin, stdout and stderr specify the executed programs' standard
input, standard output and standard error file handles, respectively.
Valid values are PIPE, an existing file descriptor (a positive
integer), an existing file object, and None.  PIPE indicates that a
new pipe to the child should be created.  With None, no redirection
will occur; the child's file handles will be inherited from the
parent.  Additionally, stderr can be STDOUT, which indicates that the
stderr data from the applications should be captured into the same
file handle as for stdout.

If preexec_fn is set to a callable object, this object will be called
in the child process just before the child is executed.

If close_fds is true, all file descriptors except 0, 1 and 2 will be
closed before the child process is executed.

if shell is true, the specified command will be executed through the
shell.

If cwd is not None, the current directory will be changed to cwd
before the child is executed.

If env is not None, it defines the environment variables for the new
process.

If universal_newlines is true, the file objects stdout and stderr are
opened as a text files, but lines may be terminated by any of '\n',
the Unix end-of-line convention, '\r', the Macintosh convention or
'\r\n', the Windows convention.  All of these external representations
are seen as '\n' by the Python program.  Note: This feature is only
available if Python is built with universal newline support (the
default).  Also, the newlines attribute of the file objects stdout,
stdin and stderr are not updated by the communicate() method.

The startupinfo and creationflags, if given, will be passed to the
underlying CreateProcess() function.  They can specify things such as
appearance of the main window and priority for the new process.
(Windows only)


This module also defines two shortcut functions:

call(*popenargs, **kwargs):
    Run command with arguments.  Wait for command to complete, then
    return the returncode attribute.

    The arguments are the same as for the Popen constructor.  Example:

    retcode = call(["ls", "-l"])

check_call(*popenargs, **kwargs):
    Run command with arguments.  Wait for command to complete.  If the
    exit code was zero then return, otherwise raise
    CalledProcessError.  The CalledProcessError object will have the
    return code in the returncode attribute.

    The arguments are the same as for the Popen constructor.  Example:

    check_call(["ls", "-l"])

Exceptions
----------
Exceptions raised in the child process, before the new program has
started to execute, will be re-raised in the parent.  Additionally,
the exception object will have one extra attribute called
'child_traceback', which is a string containing traceback information
from the childs point of view.

The most common exception raised is OSError.  This occurs, for
example, when trying to execute a non-existent file.  Applications
should prepare for OSErrors.

A ValueError will be raised if Popen is called with invalid arguments.

check_call() will raise CalledProcessError, if the called process
returns a non-zero return code.


Security
--------
Unlike some other popen functions, this implementation will never call
/bin/sh implicitly.  This means that all characters, including shell
metacharacters, can safely be passed to child processes.


Popen objects
=============
Instances of the Popen class have the following methods:

poll()
    Check if child process has terminated.  Returns returncode
    attribute.

wait()
    Wait for child process to terminate.  Returns returncode attribute.

communicate(input=None)
    Interact with process: Send data to stdin.  Read data from stdout
    and stderr, until end-of-file is reached.  Wait for process to
    terminate.  The optional stdin argument should be a string to be
    sent to the child process, or None, if no data should be sent to
    the child.

    communicate() returns a tuple (stdout, stderr).

    Note: The data read is buffered in memory, so do not use this
    method if the data size is large or unlimited.

The following attributes are also available:

stdin
    If the stdin argument is PIPE, this attribute is a file object
    that provides input to the child process.  Otherwise, it is None.

stdout
    If the stdout argument is PIPE, this attribute is a file object
    that provides output from the child process.  Otherwise, it is
    None.

stderr
    If the stderr argument is PIPE, this attribute is file object that
    provides error output from the child process.  Otherwise, it is
    None.

pid
    The process ID of the child process.

returncode
    The child return code.  A None value indicates that the process
    hasn't terminated yet.  A negative value -N indicates that the
    child was terminated by signal N (UNIX only).


Replacing older functions with the subprocess module
====================================================
In this section, "a ==> b" means that b can be used as a replacement
for a.

Note: All functions in this section fail (more or less) silently if
the executed program cannot be found; this module raises an OSError
exception.

In the following examples, we assume that the subprocess module is
imported with "from subprocess import *".


Replacing /bin/sh shell backquote
---------------------------------
output=`mycmd myarg`
==>
output = Popen(["mycmd", "myarg"], stdout=PIPE).communicate()[0]


Replacing shell pipe line
-------------------------
output=`dmesg | grep hda`
==>
p1 = Popen(["dmesg"], stdout=PIPE)
p2 = Popen(["grep", "hda"], stdin=p1.stdout, stdout=PIPE)
output = p2.communicate()[0]


Replacing os.system()
---------------------
sts = os.system("mycmd" + " myarg")
==>
p = Popen("mycmd" + " myarg", shell=True)
pid, sts = os.waitpid(p.pid, 0)

Note:

* Calling the program through the shell is usually not required.

* It's easier to look at the returncode attribute than the
  exitstatus.

A more real-world example would look like this:

try:
    retcode = call("mycmd" + " myarg", shell=True)
    if retcode < 0:
        print >>sys.stderr, "Child was terminated by signal", -retcode
    else:
        print >>sys.stderr, "Child returned", retcode
except OSError, e:
    print >>sys.stderr, "Execution failed:", e


Replacing os.spawn*
-------------------
P_NOWAIT example:

pid = os.spawnlp(os.P_NOWAIT, "/bin/mycmd", "mycmd", "myarg")
==>
pid = Popen(["/bin/mycmd", "myarg"]).pid


P_WAIT example:

retcode = os.spawnlp(os.P_WAIT, "/bin/mycmd", "mycmd", "myarg")
==>
retcode = call(["/bin/mycmd", "myarg"])


Vector example:

os.spawnvp(os.P_NOWAIT, path, args)
==>
Popen([path] + args[1:])


Environment example:

os.spawnlpe(os.P_NOWAIT, "/bin/mycmd", "mycmd", "myarg", env)
==>
Popen(["/bin/mycmd", "myarg"], env={"PATH": "/usr/bin"})


Replacing os.popen*
-------------------
pipe = os.popen(cmd, mode='r', bufsize)
==>
pipe = Popen(cmd, shell=True, bufsize=bufsize, stdout=PIPE).stdout

pipe = os.popen(cmd, mode='w', bufsize)
==>
pipe = Popen(cmd, shell=True, bufsize=bufsize, stdin=PIPE).stdin


(child_stdin, child_stdout) = os.popen2(cmd, mode, bufsize)
==>
p = Popen(cmd, shell=True, bufsize=bufsize,
          stdin=PIPE, stdout=PIPE, close_fds=True)
(child_stdin, child_stdout) = (p.stdin, p.stdout)


(child_stdin,
 child_stdout,
 child_stderr) = os.popen3(cmd, mode, bufsize)
==>
p = Popen(cmd, shell=True, bufsize=bufsize,
          stdin=PIPE, stdout=PIPE, stderr=PIPE, close_fds=True)
(child_stdin,
 child_stdout,
 child_stderr) = (p.stdin, p.stdout, p.stderr)


(child_stdin, child_stdout_and_stderr) = os.popen4(cmd, mode, bufsize)
==>
p = Popen(cmd, shell=True, bufsize=bufsize,
          stdin=PIPE, stdout=PIPE, stderr=STDOUT, close_fds=True)
(child_stdin, child_stdout_and_stderr) = (p.stdin, p.stdout)


Replacing popen2.*
------------------
Note: If the cmd argument to popen2 functions is a string, the command
is executed through /bin/sh.  If it is a list, the command is directly
executed.

(child_stdout, child_stdin) = popen2.popen2("somestring", bufsize, mode)
==>
p = Popen(["somestring"], shell=True, bufsize=bufsize
          stdin=PIPE, stdout=PIPE, close_fds=True)
(child_stdout, child_stdin) = (p.stdout, p.stdin)


(child_stdout, child_stdin) = popen2.popen2(["mycmd", "myarg"], bufsize, mode)
==>
p = Popen(["mycmd", "myarg"], bufsize=bufsize,
          stdin=PIPE, stdout=PIPE, close_fds=True)
(child_stdout, child_stdin) = (p.stdout, p.stdin)

The popen2.Popen3 and popen3.Popen4 basically works as subprocess.Popen,
except that:

* subprocess.Popen raises an exception if the execution fails
* the capturestderr argument is replaced with the stderr argument.
* stdin=PIPE and stdout=PIPE must be specified.
* popen2 closes all filedescriptors by default, but you have to specify
  close_fds=True with subprocess.Popen.


"""

import sys
mswindows = (sys.platform == "win32")

import os
import string
import types
import traceback

# Exception classes used by this module.
class CalledProcessError(Exception):
    """This exception is raised when a process run by check_call() returns
    a non-zero exit status.  The exit status will be stored in the
    returncode attribute."""
    def __init__(self, returncode, cmd):
        self.returncode = returncode
        self.cmd = cmd
    def __str__(self):
        return "Command '%s' returned non-zero exit status %d" % (self.cmd, self.returncode)


if mswindows:
    try:
        import threading
    except ImportError:
        # SCons:  the threading module is only used by the communicate()
        # method, which we don't actually use, so don't worry if we
        # can't import it.
        pass
    import msvcrt
    if 0: # <-- change this to use pywin32 instead of the _subprocess driver
        import pywintypes
        from win32api import GetStdHandle, STD_INPUT_HANDLE, \
                             STD_OUTPUT_HANDLE, STD_ERROR_HANDLE
        from win32api import GetCurrentProcess, DuplicateHandle, \
                             GetModuleFileName, GetVersion
        from win32con import DUPLICATE_SAME_ACCESS, SW_HIDE
        from win32pipe import CreatePipe
        from win32process import CreateProcess, STARTUPINFO, \
                                 GetExitCodeProcess, STARTF_USESTDHANDLES, \
                                 STARTF_USESHOWWINDOW, CREATE_NEW_CONSOLE
        from win32event import WaitForSingleObject, INFINITE, WAIT_OBJECT_0
    else:
        # SCons:  don't die on Python versions that don't have _subprocess.
        try:
            from _subprocess import *
        except ImportError:
            pass
        class STARTUPINFO:
            dwFlags = 0
            hStdInput = None
            hStdOutput = None
            hStdError = None
            wShowWindow = 0
        class pywintypes:
            error = IOError
else:
    import select
    import errno
    import fcntl
    import pickle

    try:
        fcntl.F_GETFD
    except AttributeError:
        fcntl.F_GETFD = 1

    try:
        fcntl.F_SETFD
    except AttributeError:
        fcntl.F_SETFD = 2

__all__ = ["Popen", "PIPE", "STDOUT", "call", "check_call", "CalledProcessError"]

try:
    MAXFD = os.sysconf("SC_OPEN_MAX")
except KeyboardInterrupt:
    raise       # SCons:  don't swallow keyboard interrupts
except:
    MAXFD = 256

# True/False does not exist on 2.2.0
try:
    False
except NameError:
    False = 0
    True = 1

try:
    isinstance(1, int)
except TypeError:
    def is_int(obj):
        return type(obj) == type(1)
    def is_int_or_long(obj):
        return type(obj) in (type(1), type(1L))
else:
    def is_int(obj):
        return isinstance(obj, int)
    def is_int_or_long(obj):
        return isinstance(obj, (int, long))

try:
    types.StringTypes
except AttributeError:
    try:
        types.StringTypes = (types.StringType, types.UnicodeType)
    except AttributeError:
        types.StringTypes = (types.StringType,)
    def is_string(obj):
        return type(obj) in types.StringTypes
else:
    def is_string(obj):
        return isinstance(obj, types.StringTypes)

_active = []

def _cleanup():
    for inst in _active[:]:
        if inst.poll(_deadstate=sys.maxint) >= 0:
            try:
                _active.remove(inst)
            except ValueError:
                # This can happen if two threads create a new Popen instance.
                # It's harmless that it was already removed, so ignore.
                pass

PIPE = -1
STDOUT = -2


def call(*popenargs, **kwargs):
    """Run command with arguments.  Wait for command to complete, then
    return the returncode attribute.

    The arguments are the same as for the Popen constructor.  Example:

    retcode = call(["ls", "-l"])
    """
    return apply(Popen, popenargs, kwargs).wait()


def check_call(*popenargs, **kwargs):
    """Run command with arguments.  Wait for command to complete.  If
    the exit code was zero then return, otherwise raise
    CalledProcessError.  The CalledProcessError object will have the
    return code in the returncode attribute.

    The arguments are the same as for the Popen constructor.  Example:

    check_call(["ls", "-l"])
    """
    retcode = apply(call, popenargs, kwargs)
    cmd = kwargs.get("args")
    if cmd is None:
        cmd = popenargs[0]
    if retcode:
        raise CalledProcessError(retcode, cmd)
    return retcode


def list2cmdline(seq):
    """
    Translate a sequence of arguments into a command line
    string, using the same rules as the MS C runtime:

    1) Arguments are delimited by white space, which is either a
       space or a tab.

    2) A string surrounded by double quotation marks is
       interpreted as a single argument, regardless of white space
       contained within.  A quoted string can be embedded in an
       argument.

    3) A double quotation mark preceded by a backslash is
       interpreted as a literal double quotation mark.

    4) Backslashes are interpreted literally, unless they
       immediately precede a double quotation mark.

    5) If backslashes immediately precede a double quotation mark,
       every pair of backslashes is interpreted as a literal
       backslash.  If the number of backslashes is odd, the last
       backslash escapes the next double quotation mark as
       described in rule 3.
    """

    # See
    # http://msdn.microsoft.com/library/en-us/vccelng/htm/progs_12.asp
    result = []
    needquote = False
    for arg in seq:
        bs_buf = []

        # Add a space to separate this argument from the others
        if result:
            result.append(' ')

        needquote = (" " in arg) or ("\t" in arg)
        if needquote:
            result.append('"')

        for c in arg:
            if c == '\\':
                # Don't know if we need to double yet.
                bs_buf.append(c)
            elif c == '"':
                # Double backspaces.
                result.append('\\' * len(bs_buf)*2)
                bs_buf = []
                result.append('\\"')
            else:
                # Normal char
                if bs_buf:
                    result.extend(bs_buf)
                    bs_buf = []
                result.append(c)

        # Add remaining backspaces, if any.
        if bs_buf:
            result.extend(bs_buf)

        if needquote:
            result.extend(bs_buf)
            result.append('"')

    return string.join(result, '')


try:
    object
except NameError:
    class object:
        pass

class Popen(object):
    def __init__(self, args, bufsize=0, executable=None,
                 stdin=None, stdout=None, stderr=None,
                 preexec_fn=None, close_fds=False, shell=False,
                 cwd=None, env=None, universal_newlines=False,
                 startupinfo=None, creationflags=0):
        """Create new Popen instance."""
        _cleanup()

        self._child_created = False
        if not is_int_or_long(bufsize):
            raise TypeError("bufsize must be an integer")

        if mswindows:
            if preexec_fn is not None:
                raise ValueError("preexec_fn is not supported on Windows "
                                 "platforms")
            if close_fds:
                raise ValueError("close_fds is not supported on Windows "
                                 "platforms")
        else:
            # POSIX
            if startupinfo is not None:
                raise ValueError("startupinfo is only supported on Windows "
                                 "platforms")
            if creationflags != 0:
                raise ValueError("creationflags is only supported on Windows "
                                 "platforms")

        self.stdin = None
        self.stdout = None
        self.stderr = None
        self.pid = None
        self.returncode = None
        self.universal_newlines = universal_newlines

        # Input and output objects. The general principle is like
        # this:
        #
        # Parent                   Child
        # ------                   -----
        # p2cwrite   ---stdin--->  p2cread
        # c2pread    <--stdout---  c2pwrite
        # errread    <--stderr---  errwrite
        #
        # On POSIX, the child objects are file descriptors.  On
        # Windows, these are Windows file handles.  The parent objects
        # are file descriptors on both platforms.  The parent objects
        # are None when not using PIPEs. The child objects are None
        # when not redirecting.

        (p2cread, p2cwrite,
         c2pread, c2pwrite,
         errread, errwrite) = self._get_handles(stdin, stdout, stderr)

        self._execute_child(args, executable, preexec_fn, close_fds,
                            cwd, env, universal_newlines,
                            startupinfo, creationflags, shell,
                            p2cread, p2cwrite,
                            c2pread, c2pwrite,
                            errread, errwrite)

        if p2cwrite:
            self.stdin = os.fdopen(p2cwrite, 'wb', bufsize)
        if c2pread:
            if universal_newlines:
                self.stdout = os.fdopen(c2pread, 'rU', bufsize)
            else:
                self.stdout = os.fdopen(c2pread, 'rb', bufsize)
        if errread:
            if universal_newlines:
                self.stderr = os.fdopen(errread, 'rU', bufsize)
            else:
                self.stderr = os.fdopen(errread, 'rb', bufsize)


    def _translate_newlines(self, data):
        data = data.replace("\r\n", "\n")
        data = data.replace("\r", "\n")
        return data


    def __del__(self):
        if not self._child_created:
            # We didn't get to successfully create a child process.
            return
        # In case the child hasn't been waited on, check if it's done.
        self.poll(_deadstate=sys.maxint)
        if self.returncode is None and _active is not None:
            # Child is still running, keep us alive until we can wait on it.
            _active.append(self)


    def communicate(self, input=None):
        """Interact with process: Send data to stdin.  Read data from
        stdout and stderr, until end-of-file is reached.  Wait for
        process to terminate.  The optional input argument should be a
        string to be sent to the child process, or None, if no data
        should be sent to the child.

        communicate() returns a tuple (stdout, stderr)."""

        # Optimization: If we are only using one pipe, or no pipe at
        # all, using select() or threads is unnecessary.
        if [self.stdin, self.stdout, self.stderr].count(None) >= 2:
            stdout = None
            stderr = None
            if self.stdin:
                if input:
                    self.stdin.write(input)
                self.stdin.close()
            elif self.stdout:
                stdout = self.stdout.read()
            elif self.stderr:
                stderr = self.stderr.read()
            self.wait()
            return (stdout, stderr)

        return self._communicate(input)


    if mswindows:
        #
        # Windows methods
        #
        def _get_handles(self, stdin, stdout, stderr):
            """Construct and return tupel with IO objects:
            p2cread, p2cwrite, c2pread, c2pwrite, errread, errwrite
            """
            if stdin is None and stdout is None and stderr is None:
                return (None, None, None, None, None, None)

            p2cread, p2cwrite = None, None
            c2pread, c2pwrite = None, None
            errread, errwrite = None, None

            if stdin is None:
                p2cread = GetStdHandle(STD_INPUT_HANDLE)
            elif stdin == PIPE:
                p2cread, p2cwrite = CreatePipe(None, 0)
                # Detach and turn into fd
                p2cwrite = p2cwrite.Detach()
                p2cwrite = msvcrt.open_osfhandle(p2cwrite, 0)
            elif is_int(stdin):
                p2cread = msvcrt.get_osfhandle(stdin)
            else:
                # Assuming file-like object
                p2cread = msvcrt.get_osfhandle(stdin.fileno())
            p2cread = self._make_inheritable(p2cread)

            if stdout is None:
                c2pwrite = GetStdHandle(STD_OUTPUT_HANDLE)
            elif stdout == PIPE:
                c2pread, c2pwrite = CreatePipe(None, 0)
                # Detach and turn into fd
                c2pread = c2pread.Detach()
                c2pread = msvcrt.open_osfhandle(c2pread, 0)
            elif is_int(stdout):
                c2pwrite = msvcrt.get_osfhandle(stdout)
            else:
                # Assuming file-like object
                c2pwrite = msvcrt.get_osfhandle(stdout.fileno())
            c2pwrite = self._make_inheritable(c2pwrite)

            if stderr is None:
                errwrite = GetStdHandle(STD_ERROR_HANDLE)
            elif stderr == PIPE:
                errread, errwrite = CreatePipe(None, 0)
                # Detach and turn into fd
                errread = errread.Detach()
                errread = msvcrt.open_osfhandle(errread, 0)
            elif stderr == STDOUT:
                errwrite = c2pwrite
            elif is_int(stderr):
                errwrite = msvcrt.get_osfhandle(stderr)
            else:
                # Assuming file-like object
                errwrite = msvcrt.get_osfhandle(stderr.fileno())
            errwrite = self._make_inheritable(errwrite)

            return (p2cread, p2cwrite,
                    c2pread, c2pwrite,
                    errread, errwrite)


        def _make_inheritable(self, handle):
            """Return a duplicate of handle, which is inheritable"""
            return DuplicateHandle(GetCurrentProcess(), handle,
                                   GetCurrentProcess(), 0, 1,
                                   DUPLICATE_SAME_ACCESS)


        def _find_w9xpopen(self):
            """Find and return absolut path to w9xpopen.exe"""
            w9xpopen = os.path.join(os.path.dirname(GetModuleFileName(0)),
                                    "w9xpopen.exe")
            if not os.path.exists(w9xpopen):
                # Eeek - file-not-found - possibly an embedding
                # situation - see if we can locate it in sys.exec_prefix
                w9xpopen = os.path.join(os.path.dirname(sys.exec_prefix),
                                        "w9xpopen.exe")
                if not os.path.exists(w9xpopen):
                    raise RuntimeError("Cannot locate w9xpopen.exe, which is "
                                       "needed for Popen to work with your "
                                       "shell or platform.")
            return w9xpopen


        def _execute_child(self, args, executable, preexec_fn, close_fds,
                           cwd, env, universal_newlines,
                           startupinfo, creationflags, shell,
                           p2cread, p2cwrite,
                           c2pread, c2pwrite,
                           errread, errwrite):
            """Execute program (MS Windows version)"""

            if not isinstance(args, types.StringTypes):
                args = list2cmdline(args)

            # Process startup details
            if startupinfo is None:
                startupinfo = STARTUPINFO()
            if None not in (p2cread, c2pwrite, errwrite):
                startupinfo.dwFlags = startupinfo.dwFlags | STARTF_USESTDHANDLES
                startupinfo.hStdInput = p2cread
                startupinfo.hStdOutput = c2pwrite
                startupinfo.hStdError = errwrite

            if shell:
                startupinfo.dwFlags = startupinfo.dwFlags | STARTF_USESHOWWINDOW
                startupinfo.wShowWindow = SW_HIDE
                comspec = os.environ.get("COMSPEC", "cmd.exe")
                args = comspec + " /c " + args
                if (GetVersion() >= 0x80000000L or
                        os.path.basename(comspec).lower() == "command.com"):
                    # Win9x, or using command.com on NT. We need to
                    # use the w9xpopen intermediate program. For more
                    # information, see KB Q150956
                    # (http://web.archive.org/web/20011105084002/http://support.microsoft.com/support/kb/articles/Q150/9/56.asp)
                    w9xpopen = self._find_w9xpopen()
                    args = '"%s" %s' % (w9xpopen, args)
                    # Not passing CREATE_NEW_CONSOLE has been known to
                    # cause random failures on win9x.  Specifically a
                    # dialog: "Your program accessed mem currently in
                    # use at xxx" and a hopeful warning about the
                    # stability of your system.  Cost is Ctrl+C wont
                    # kill children.
                    creationflags = creationflags | CREATE_NEW_CONSOLE

            # Start the process
            try:
                hp, ht, pid, tid = CreateProcess(executable, args,
                                         # no special security
                                         None, None,
                                         # must inherit handles to pass std
                                         # handles
                                         1,
                                         creationflags,
                                         env,
                                         cwd,
                                         startupinfo)
            except pywintypes.error, e:
                # Translate pywintypes.error to WindowsError, which is
                # a subclass of OSError.  FIXME: We should really
                # translate errno using _sys_errlist (or simliar), but
                # how can this be done from Python?
                raise apply(WindowsError, e.args)

            # Retain the process handle, but close the thread handle
            self._child_created = True
            self._handle = hp
            self.pid = pid
            ht.Close()

            # Child is launched. Close the parent's copy of those pipe
            # handles that only the child should have open.  You need
            # to make sure that no handles to the write end of the
            # output pipe are maintained in this process or else the
            # pipe will not close when the child process exits and the
            # ReadFile will hang.
            if p2cread is not None:
                p2cread.Close()
            if c2pwrite is not None:
                c2pwrite.Close()
            if errwrite is not None:
                errwrite.Close()


        def poll(self, _deadstate=None):
            """Check if child process has terminated.  Returns returncode
            attribute."""
            if self.returncode is None:
                if WaitForSingleObject(self._handle, 0) == WAIT_OBJECT_0:
                    self.returncode = GetExitCodeProcess(self._handle)
            return self.returncode


        def wait(self):
            """Wait for child process to terminate.  Returns returncode
            attribute."""
            if self.returncode is None:
                obj = WaitForSingleObject(self._handle, INFINITE)
                self.returncode = GetExitCodeProcess(self._handle)
            return self.returncode


        def _readerthread(self, fh, buffer):
            buffer.append(fh.read())


        def _communicate(self, input):
            stdout = None # Return
            stderr = None # Return

            if self.stdout:
                stdout = []
                stdout_thread = threading.Thread(target=self._readerthread,
                                                 args=(self.stdout, stdout))
                stdout_thread.setDaemon(True)
                stdout_thread.start()
            if self.stderr:
                stderr = []
                stderr_thread = threading.Thread(target=self._readerthread,
                                                 args=(self.stderr, stderr))
                stderr_thread.setDaemon(True)
                stderr_thread.start()

            if self.stdin:
                if input is not None:
                    self.stdin.write(input)
                self.stdin.close()

            if self.stdout:
                stdout_thread.join()
            if self.stderr:
                stderr_thread.join()

            # All data exchanged.  Translate lists into strings.
            if stdout is not None:
                stdout = stdout[0]
            if stderr is not None:
                stderr = stderr[0]

            # Translate newlines, if requested.  We cannot let the file
            # object do the translation: It is based on stdio, which is
            # impossible to combine with select (unless forcing no
            # buffering).
            if self.universal_newlines and hasattr(file, 'newlines'):
                if stdout:
                    stdout = self._translate_newlines(stdout)
                if stderr:
                    stderr = self._translate_newlines(stderr)

            self.wait()
            return (stdout, stderr)

    else:
        #
        # POSIX methods
        #
        def _get_handles(self, stdin, stdout, stderr):
            """Construct and return tupel with IO objects:
            p2cread, p2cwrite, c2pread, c2pwrite, errread, errwrite
            """
            p2cread, p2cwrite = None, None
            c2pread, c2pwrite = None, None
            errread, errwrite = None, None

            if stdin is None:
                pass
            elif stdin == PIPE:
                p2cread, p2cwrite = os.pipe()
            elif is_int(stdin):
                p2cread = stdin
            else:
                # Assuming file-like object
                p2cread = stdin.fileno()

            if stdout is None:
                pass
            elif stdout == PIPE:
                c2pread, c2pwrite = os.pipe()
            elif is_int(stdout):
                c2pwrite = stdout
            else:
                # Assuming file-like object
                c2pwrite = stdout.fileno()

            if stderr is None:
                pass
            elif stderr == PIPE:
                errread, errwrite = os.pipe()
            elif stderr == STDOUT:
                errwrite = c2pwrite
            elif is_int(stderr):
                errwrite = stderr
            else:
                # Assuming file-like object
                errwrite = stderr.fileno()

            return (p2cread, p2cwrite,
                    c2pread, c2pwrite,
                    errread, errwrite)


        def _set_cloexec_flag(self, fd):
            try:
                cloexec_flag = fcntl.FD_CLOEXEC
            except AttributeError:
                cloexec_flag = 1

            old = fcntl.fcntl(fd, fcntl.F_GETFD)
            fcntl.fcntl(fd, fcntl.F_SETFD, old | cloexec_flag)


        def _close_fds(self, but):
            for i in xrange(3, MAXFD):
                if i == but:
                    continue
                try:
                    os.close(i)
                except KeyboardInterrupt:
                    raise       # SCons:  don't swallow keyboard interrupts
                except:
                    pass


        def _execute_child(self, args, executable, preexec_fn, close_fds,
                           cwd, env, universal_newlines,
                           startupinfo, creationflags, shell,
                           p2cread, p2cwrite,
                           c2pread, c2pwrite,
                           errread, errwrite):
            """Execute program (POSIX version)"""

            if is_string(args):
                args = [args]

            if shell:
                args = ["/bin/sh", "-c"] + args

            if executable is None:
                executable = args[0]

            # For transferring possible exec failure from child to parent
            # The first char specifies the exception type: 0 means
            # OSError, 1 means some other error.
            errpipe_read, errpipe_write = os.pipe()
            self._set_cloexec_flag(errpipe_write)

            self.pid = os.fork()
            self._child_created = True
            if self.pid == 0:
                # Child
                try:
                    # Close parent's pipe ends
                    if p2cwrite:
                        os.close(p2cwrite)
                    if c2pread:
                        os.close(c2pread)
                    if errread:
                        os.close(errread)
                    os.close(errpipe_read)

                    # Dup fds for child
                    if p2cread:
                        os.dup2(p2cread, 0)
                    if c2pwrite:
                        os.dup2(c2pwrite, 1)
                    if errwrite:
                        os.dup2(errwrite, 2)

                    # Close pipe fds.  Make sure we don't close the same
                    # fd more than once, or standard fds.
                    try:
                        set
                    except NameError:
                        # Fall-back for earlier Python versions, so epydoc
                        # can use this module directly to execute things.
                        if p2cread:
                            os.close(p2cread)
                        if c2pwrite and c2pwrite not in (p2cread,):
                            os.close(c2pwrite)
                        if errwrite and errwrite not in (p2cread, c2pwrite):
                            os.close(errwrite)
                    else:
                        for fd in set((p2cread, c2pwrite, errwrite))-set((0,1,2)):
                            if fd: os.close(fd)

                    # Close all other fds, if asked for
                    if close_fds:
                        self._close_fds(but=errpipe_write)

                    if cwd is not None:
                        os.chdir(cwd)

                    if preexec_fn:
                        apply(preexec_fn)

                    if env is None:
                        os.execvp(executable, args)
                    else:
                        os.execvpe(executable, args, env)

                except KeyboardInterrupt:
                    raise       # SCons:  don't swallow keyboard interrupts

                except:
                    exc_type, exc_value, tb = sys.exc_info()
                    # Save the traceback and attach it to the exception object
                    exc_lines = traceback.format_exception(exc_type,
                                                           exc_value,
                                                           tb)
                    exc_value.child_traceback = string.join(exc_lines, '')
                    os.write(errpipe_write, pickle.dumps(exc_value))

                # This exitcode won't be reported to applications, so it
                # really doesn't matter what we return.
                os._exit(255)

            # Parent
            os.close(errpipe_write)
            if p2cread and p2cwrite:
                os.close(p2cread)
            if c2pwrite and c2pread:
                os.close(c2pwrite)
            if errwrite and errread:
                os.close(errwrite)

            # Wait for exec to fail or succeed; possibly raising exception
            data = os.read(errpipe_read, 1048576) # Exceptions limited to 1 MB
            os.close(errpipe_read)
            if data != "":
                os.waitpid(self.pid, 0)
                child_exception = pickle.loads(data)
                raise child_exception


        def _handle_exitstatus(self, sts):
            if os.WIFSIGNALED(sts):
                self.returncode = -os.WTERMSIG(sts)
            elif os.WIFEXITED(sts):
                self.returncode = os.WEXITSTATUS(sts)
            else:
                # Should never happen
                raise RuntimeError("Unknown child exit status!")


        def poll(self, _deadstate=None):
            """Check if child process has terminated.  Returns returncode
            attribute."""
            if self.returncode is None:
                try:
                    pid, sts = os.waitpid(self.pid, os.WNOHANG)
                    if pid == self.pid:
                        self._handle_exitstatus(sts)
                except os.error:
                    if _deadstate is not None:
                        self.returncode = _deadstate
            return self.returncode


        def wait(self):
            """Wait for child process to terminate.  Returns returncode
            attribute."""
            if self.returncode is None:
                pid, sts = os.waitpid(self.pid, 0)
                self._handle_exitstatus(sts)
            return self.returncode


        def _communicate(self, input):
            read_set = []
            write_set = []
            stdout = None # Return
            stderr = None # Return

            if self.stdin:
                # Flush stdio buffer.  This might block, if the user has
                # been writing to .stdin in an uncontrolled fashion.
                self.stdin.flush()
                if input:
                    write_set.append(self.stdin)
                else:
                    self.stdin.close()
            if self.stdout:
                read_set.append(self.stdout)
                stdout = []
            if self.stderr:
                read_set.append(self.stderr)
                stderr = []

            input_offset = 0
            while read_set or write_set:
                rlist, wlist, xlist = select.select(read_set, write_set, [])

                if self.stdin in wlist:
                    # When select has indicated that the file is writable,
                    # we can write up to PIPE_BUF bytes without risk
                    # blocking.  POSIX defines PIPE_BUF >= 512
                    bytes_written = os.write(self.stdin.fileno(), buffer(input, input_offset, 512))
                    input_offset = input_offset + bytes_written
                    if input_offset >= len(input):
                        self.stdin.close()
                        write_set.remove(self.stdin)

                if self.stdout in rlist:
                    data = os.read(self.stdout.fileno(), 1024)
                    if data == "":
                        self.stdout.close()
                        read_set.remove(self.stdout)
                    stdout.append(data)

                if self.stderr in rlist:
                    data = os.read(self.stderr.fileno(), 1024)
                    if data == "":
                        self.stderr.close()
                        read_set.remove(self.stderr)
                    stderr.append(data)

            # All data exchanged.  Translate lists into strings.
            if stdout is not None:
                stdout = string.join(stdout, '')
            if stderr is not None:
                stderr = string.join(stderr, '')

            # Translate newlines, if requested.  We cannot let the file
            # object do the translation: It is based on stdio, which is
            # impossible to combine with select (unless forcing no
            # buffering).
            if self.universal_newlines and hasattr(file, 'newlines'):
                if stdout:
                    stdout = self._translate_newlines(stdout)
                if stderr:
                    stderr = self._translate_newlines(stderr)

            self.wait()
            return (stdout, stderr)


def _demo_posix():
    #
    # Example 1: Simple redirection: Get process list
    #
    plist = Popen(["ps"], stdout=PIPE).communicate()[0]
    print "Process list:"
    print plist

    #
    # Example 2: Change uid before executing child
    #
    if os.getuid() == 0:
        p = Popen(["id"], preexec_fn=lambda: os.setuid(100))
        p.wait()

    #
    # Example 3: Connecting several subprocesses
    #
    print "Looking for 'hda'..."
    p1 = Popen(["dmesg"], stdout=PIPE)
    p2 = Popen(["grep", "hda"], stdin=p1.stdout, stdout=PIPE)
    print repr(p2.communicate()[0])

    #
    # Example 4: Catch execution error
    #
    print
    print "Trying a weird file..."
    try:
        print Popen(["/this/path/does/not/exist"]).communicate()
    except OSError, e:
        if e.errno == errno.ENOENT:
            print "The file didn't exist.  I thought so..."
            print "Child traceback:"
            print e.child_traceback
        else:
            print "Error", e.errno
    else:
        sys.stderr.write( "Gosh.  No error.\n" )


def _demo_windows():
    #
    # Example 1: Connecting several subprocesses
    #
    print "Looking for 'PROMPT' in set output..."
    p1 = Popen("set", stdout=PIPE, shell=True)
    p2 = Popen('find "PROMPT"', stdin=p1.stdout, stdout=PIPE)
    print repr(p2.communicate()[0])

    #
    # Example 2: Simple execution of program
    #
    print "Executing calc..."
    p = Popen("calc")
    p.wait()


if __name__ == "__main__":
    if mswindows:
        _demo_windows()
    else:
        _demo_posix()
