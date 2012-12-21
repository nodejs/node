"""
TestCmd.py:  a testing framework for commands and scripts.

The TestCmd module provides a framework for portable automated testing
of executable commands and scripts (in any language, not just Python),
especially commands and scripts that require file system interaction.

In addition to running tests and evaluating conditions, the TestCmd
module manages and cleans up one or more temporary workspace
directories, and provides methods for creating files and directories in
those workspace directories from in-line data, here-documents), allowing
tests to be completely self-contained.

A TestCmd environment object is created via the usual invocation:

    import TestCmd
    test = TestCmd.TestCmd()

There are a bunch of keyword arguments available at instantiation:

    test = TestCmd.TestCmd(description = 'string',
                           program = 'program_or_script_to_test',
                           interpreter = 'script_interpreter',
                           workdir = 'prefix',
                           subdir = 'subdir',
                           verbose = Boolean,
                           match = default_match_function,
                           diff = default_diff_function,
                           combine = Boolean)

There are a bunch of methods that let you do different things:

    test.verbose_set(1)

    test.description_set('string')

    test.program_set('program_or_script_to_test')

    test.interpreter_set('script_interpreter')
    test.interpreter_set(['script_interpreter', 'arg'])

    test.workdir_set('prefix')
    test.workdir_set('')

    test.workpath('file')
    test.workpath('subdir', 'file')

    test.subdir('subdir', ...)

    test.rmdir('subdir', ...)

    test.write('file', "contents\n")
    test.write(['subdir', 'file'], "contents\n")

    test.read('file')
    test.read(['subdir', 'file'])
    test.read('file', mode)
    test.read(['subdir', 'file'], mode)

    test.writable('dir', 1)
    test.writable('dir', None)

    test.preserve(condition, ...)

    test.cleanup(condition)

    test.command_args(program = 'program_or_script_to_run',
                      interpreter = 'script_interpreter',
                      arguments = 'arguments to pass to program')

    test.run(program = 'program_or_script_to_run',
             interpreter = 'script_interpreter',
             arguments = 'arguments to pass to program',
             chdir = 'directory_to_chdir_to',
             stdin = 'input to feed to the program\n')
             universal_newlines = True)

    p = test.start(program = 'program_or_script_to_run',
                   interpreter = 'script_interpreter',
                   arguments = 'arguments to pass to program',
                   universal_newlines = None)

    test.finish(self, p)

    test.pass_test()
    test.pass_test(condition)
    test.pass_test(condition, function)

    test.fail_test()
    test.fail_test(condition)
    test.fail_test(condition, function)
    test.fail_test(condition, function, skip)

    test.no_result()
    test.no_result(condition)
    test.no_result(condition, function)
    test.no_result(condition, function, skip)

    test.stdout()
    test.stdout(run)

    test.stderr()
    test.stderr(run)

    test.symlink(target, link)

    test.banner(string)
    test.banner(string, width)

    test.diff(actual, expected)

    test.match(actual, expected)

    test.match_exact("actual 1\nactual 2\n", "expected 1\nexpected 2\n")
    test.match_exact(["actual 1\n", "actual 2\n"],
                     ["expected 1\n", "expected 2\n"])

    test.match_re("actual 1\nactual 2\n", regex_string)
    test.match_re(["actual 1\n", "actual 2\n"], list_of_regexes)

    test.match_re_dotall("actual 1\nactual 2\n", regex_string)
    test.match_re_dotall(["actual 1\n", "actual 2\n"], list_of_regexes)

    test.tempdir()
    test.tempdir('temporary-directory')

    test.sleep()
    test.sleep(seconds)

    test.where_is('foo')
    test.where_is('foo', 'PATH1:PATH2')
    test.where_is('foo', 'PATH1;PATH2', '.suffix3;.suffix4')

    test.unlink('file')
    test.unlink('subdir', 'file')

The TestCmd module provides pass_test(), fail_test(), and no_result()
unbound functions that report test results for use with the Aegis change
management system.  These methods terminate the test immediately,
reporting PASSED, FAILED, or NO RESULT respectively, and exiting with
status 0 (success), 1 or 2 respectively.  This allows for a distinction
between an actual failed test and a test that could not be properly
evaluated because of an external condition (such as a full file system
or incorrect permissions).

    import TestCmd

    TestCmd.pass_test()
    TestCmd.pass_test(condition)
    TestCmd.pass_test(condition, function)

    TestCmd.fail_test()
    TestCmd.fail_test(condition)
    TestCmd.fail_test(condition, function)
    TestCmd.fail_test(condition, function, skip)

    TestCmd.no_result()
    TestCmd.no_result(condition)
    TestCmd.no_result(condition, function)
    TestCmd.no_result(condition, function, skip)

The TestCmd module also provides unbound functions that handle matching
in the same way as the match_*() methods described above.

    import TestCmd

    test = TestCmd.TestCmd(match = TestCmd.match_exact)

    test = TestCmd.TestCmd(match = TestCmd.match_re)

    test = TestCmd.TestCmd(match = TestCmd.match_re_dotall)

The TestCmd module provides unbound functions that can be used for the
"diff" argument to TestCmd.TestCmd instantiation:

    import TestCmd

    test = TestCmd.TestCmd(match = TestCmd.match_re,
                           diff = TestCmd.diff_re)

    test = TestCmd.TestCmd(diff = TestCmd.simple_diff)

The "diff" argument can also be used with standard difflib functions:

    import difflib

    test = TestCmd.TestCmd(diff = difflib.context_diff)

    test = TestCmd.TestCmd(diff = difflib.unified_diff)

Lastly, the where_is() method also exists in an unbound function
version.

    import TestCmd

    TestCmd.where_is('foo')
    TestCmd.where_is('foo', 'PATH1:PATH2')
    TestCmd.where_is('foo', 'PATH1;PATH2', '.suffix3;.suffix4')
"""

# Copyright 2000-2010 Steven Knight
# This module is free software, and you may redistribute it and/or modify
# it under the same terms as Python itself, so long as this copyright message
# and disclaimer are retained in their original form.
#
# IN NO EVENT SHALL THE AUTHOR BE LIABLE TO ANY PARTY FOR DIRECT, INDIRECT,
# SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OF
# THIS CODE, EVEN IF THE AUTHOR HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH
# DAMAGE.
#
# THE AUTHOR SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
# PARTICULAR PURPOSE.  THE CODE PROVIDED HEREUNDER IS ON AN "AS IS" BASIS,
# AND THERE IS NO OBLIGATION WHATSOEVER TO PROVIDE MAINTENANCE,
# SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.

__author__ = "Steven Knight <knight at baldmt dot com>"
__revision__ = "TestCmd.py 0.37.D001 2010/01/11 16:55:50 knight"
__version__ = "0.37"

import errno
import os
import os.path
import re
import shutil
import stat
import string
import sys
import tempfile
import time
import traceback
import types
import UserList

__all__ = [
    'diff_re',
    'fail_test',
    'no_result',
    'pass_test',
    'match_exact',
    'match_re',
    'match_re_dotall',
    'python_executable',
    'TestCmd'
]

try:
    import difflib
except ImportError:
    __all__.append('simple_diff')

def is_List(e):
    return type(e) is types.ListType \
        or isinstance(e, UserList.UserList)

try:
    from UserString import UserString
except ImportError:
    class UserString:
        pass

if hasattr(types, 'UnicodeType'):
    def is_String(e):
        return type(e) is types.StringType \
            or type(e) is types.UnicodeType \
            or isinstance(e, UserString)
else:
    def is_String(e):
        return type(e) is types.StringType or isinstance(e, UserString)

tempfile.template = 'testcmd.'
if os.name in ('posix', 'nt'):
    tempfile.template = 'testcmd.' + str(os.getpid()) + '.'
else:
    tempfile.template = 'testcmd.'

re_space = re.compile('\s')

_Cleanup = []

_chain_to_exitfunc = None

def _clean():
    global _Cleanup
    cleanlist = filter(None, _Cleanup)
    del _Cleanup[:]
    cleanlist.reverse()
    for test in cleanlist:
        test.cleanup()
    if _chain_to_exitfunc:
        _chain_to_exitfunc()

try:
    import atexit
except ImportError:
    # TODO(1.5): atexit requires python 2.0, so chain sys.exitfunc
    try:
        _chain_to_exitfunc = sys.exitfunc
    except AttributeError:
        pass
    sys.exitfunc = _clean
else:
    atexit.register(_clean)

try:
    zip
except NameError:
    def zip(*lists):
        result = []
        for i in xrange(min(map(len, lists))):
            result.append(tuple(map(lambda l, i=i: l[i], lists)))
        return result

class Collector:
    def __init__(self, top):
        self.entries = [top]
    def __call__(self, arg, dirname, names):
        pathjoin = lambda n, d=dirname: os.path.join(d, n)
        self.entries.extend(map(pathjoin, names))

def _caller(tblist, skip):
    string = ""
    arr = []
    for file, line, name, text in tblist:
        if file[-10:] == "TestCmd.py":
                break
        arr = [(file, line, name, text)] + arr
    atfrom = "at"
    for file, line, name, text in arr[skip:]:
        if name in ("?", "<module>"):
            name = ""
        else:
            name = " (" + name + ")"
        string = string + ("%s line %d of %s%s\n" % (atfrom, line, file, name))
        atfrom = "\tfrom"
    return string

def fail_test(self = None, condition = 1, function = None, skip = 0):
    """Cause the test to fail.

    By default, the fail_test() method reports that the test FAILED
    and exits with a status of 1.  If a condition argument is supplied,
    the test fails only if the condition is true.
    """
    if not condition:
        return
    if not function is None:
        function()
    of = ""
    desc = ""
    sep = " "
    if not self is None:
        if self.program:
            of = " of " + self.program
            sep = "\n\t"
        if self.description:
            desc = " [" + self.description + "]"
            sep = "\n\t"

    at = _caller(traceback.extract_stack(), skip)
    sys.stderr.write("FAILED test" + of + desc + sep + at)

    sys.exit(1)

def no_result(self = None, condition = 1, function = None, skip = 0):
    """Causes a test to exit with no valid result.

    By default, the no_result() method reports NO RESULT for the test
    and exits with a status of 2.  If a condition argument is supplied,
    the test fails only if the condition is true.
    """
    if not condition:
        return
    if not function is None:
        function()
    of = ""
    desc = ""
    sep = " "
    if not self is None:
        if self.program:
            of = " of " + self.program
            sep = "\n\t"
        if self.description:
            desc = " [" + self.description + "]"
            sep = "\n\t"

    if os.environ.get('TESTCMD_DEBUG_SKIPS'):
        at = _caller(traceback.extract_stack(), skip)
        sys.stderr.write("NO RESULT for test" + of + desc + sep + at)
    else:
        sys.stderr.write("NO RESULT\n")

    sys.exit(2)

def pass_test(self = None, condition = 1, function = None):
    """Causes a test to pass.

    By default, the pass_test() method reports PASSED for the test
    and exits with a status of 0.  If a condition argument is supplied,
    the test passes only if the condition is true.
    """
    if not condition:
        return
    if not function is None:
        function()
    sys.stderr.write("PASSED\n")
    sys.exit(0)

def match_exact(lines = None, matches = None):
    """
    """
    if not is_List(lines):
        lines = string.split(lines, "\n")
    if not is_List(matches):
        matches = string.split(matches, "\n")
    if len(lines) != len(matches):
        return
    for i in range(len(lines)):
        if lines[i] != matches[i]:
            return
    return 1

def match_re(lines = None, res = None):
    """
    """
    if not is_List(lines):
        lines = string.split(lines, "\n")
    if not is_List(res):
        res = string.split(res, "\n")
    if len(lines) != len(res):
        return
    for i in range(len(lines)):
        s = "^" + res[i] + "$"
        try:
            expr = re.compile(s)
        except re.error, e:
            msg = "Regular expression error in %s: %s"
            raise re.error, msg % (repr(s), e[0])
        if not expr.search(lines[i]):
            return
    return 1

def match_re_dotall(lines = None, res = None):
    """
    """
    if not type(lines) is type(""):
        lines = string.join(lines, "\n")
    if not type(res) is type(""):
        res = string.join(res, "\n")
    s = "^" + res + "$"
    try:
        expr = re.compile(s, re.DOTALL)
    except re.error, e:
        msg = "Regular expression error in %s: %s"
        raise re.error, msg % (repr(s), e[0])
    if expr.match(lines):
        return 1

try:
    import difflib
except ImportError:
    pass
else:
    def simple_diff(a, b, fromfile='', tofile='',
                    fromfiledate='', tofiledate='', n=3, lineterm='\n'):
        """
        A function with the same calling signature as difflib.context_diff
        (diff -c) and difflib.unified_diff (diff -u) but which prints
        output like the simple, unadorned 'diff" command.
        """
        sm = difflib.SequenceMatcher(None, a, b)
        def comma(x1, x2):
            return x1+1 == x2 and str(x2) or '%s,%s' % (x1+1, x2)
        result = []
        for op, a1, a2, b1, b2 in sm.get_opcodes():
            if op == 'delete':
                result.append("%sd%d" % (comma(a1, a2), b1))
                result.extend(map(lambda l: '< ' + l, a[a1:a2]))
            elif op == 'insert':
                result.append("%da%s" % (a1, comma(b1, b2)))
                result.extend(map(lambda l: '> ' + l, b[b1:b2]))
            elif op == 'replace':
                result.append("%sc%s" % (comma(a1, a2), comma(b1, b2)))
                result.extend(map(lambda l: '< ' + l, a[a1:a2]))
                result.append('---')
                result.extend(map(lambda l: '> ' + l, b[b1:b2]))
        return result

def diff_re(a, b, fromfile='', tofile='',
                fromfiledate='', tofiledate='', n=3, lineterm='\n'):
    """
    A simple "diff" of two sets of lines when the expected lines
    are regular expressions.  This is a really dumb thing that
    just compares each line in turn, so it doesn't look for
    chunks of matching lines and the like--but at least it lets
    you know exactly which line first didn't compare correctl...
    """
    result = []
    diff = len(a) - len(b)
    if diff < 0:
        a = a + ['']*(-diff)
    elif diff > 0:
        b = b + ['']*diff
    i = 0
    for aline, bline in zip(a, b):
        s = "^" + aline + "$"
        try:
            expr = re.compile(s)
        except re.error, e:
            msg = "Regular expression error in %s: %s"
            raise re.error, msg % (repr(s), e[0])
        if not expr.search(bline):
            result.append("%sc%s" % (i+1, i+1))
            result.append('< ' + repr(a[i]))
            result.append('---')
            result.append('> ' + repr(b[i]))
        i = i+1
    return result

if os.name == 'java':

    python_executable = os.path.join(sys.prefix, 'jython')

else:

    python_executable = sys.executable

if sys.platform == 'win32':

    default_sleep_seconds = 2

    def where_is(file, path=None, pathext=None):
        if path is None:
            path = os.environ['PATH']
        if is_String(path):
            path = string.split(path, os.pathsep)
        if pathext is None:
            pathext = os.environ['PATHEXT']
        if is_String(pathext):
            pathext = string.split(pathext, os.pathsep)
        for ext in pathext:
            if string.lower(ext) == string.lower(file[-len(ext):]):
                pathext = ['']
                break
        for dir in path:
            f = os.path.join(dir, file)
            for ext in pathext:
                fext = f + ext
                if os.path.isfile(fext):
                    return fext
        return None

else:

    def where_is(file, path=None, pathext=None):
        if path is None:
            path = os.environ['PATH']
        if is_String(path):
            path = string.split(path, os.pathsep)
        for dir in path:
            f = os.path.join(dir, file)
            if os.path.isfile(f):
                try:
                    st = os.stat(f)
                except OSError:
                    continue
                if stat.S_IMODE(st[stat.ST_MODE]) & 0111:
                    return f
        return None

    default_sleep_seconds = 1



try:
    import subprocess
except ImportError:
    # The subprocess module doesn't exist in this version of Python,
    # so we're going to cobble up something that looks just enough
    # like its API for our purposes below.
    import new

    subprocess = new.module('subprocess')

    subprocess.PIPE = 'PIPE'
    subprocess.STDOUT = 'STDOUT'
    subprocess.mswindows = (sys.platform == 'win32')

    try:
        import popen2
        popen2.Popen3
    except AttributeError:
        class Popen3:
            universal_newlines = 1
            def __init__(self, command, **kw):
                if sys.platform == 'win32' and command[0] == '"':
                    command = '"' + command + '"'
                (stdin, stdout, stderr) = os.popen3(' ' + command)
                self.stdin = stdin
                self.stdout = stdout
                self.stderr = stderr
            def close_output(self):
                self.stdout.close()
                self.resultcode = self.stderr.close()
            def wait(self):
                resultcode = self.resultcode
                if os.WIFEXITED(resultcode):
                    return os.WEXITSTATUS(resultcode)
                elif os.WIFSIGNALED(resultcode):
                    return os.WTERMSIG(resultcode)
                else:
                    return None

    else:
        try:
            popen2.Popen4
        except AttributeError:
            # A cribbed Popen4 class, with some retrofitted code from
            # the Python 1.5 Popen3 class methods to do certain things
            # by hand.
            class Popen4(popen2.Popen3):
                childerr = None

                def __init__(self, cmd, bufsize=-1):
                    p2cread, p2cwrite = os.pipe()
                    c2pread, c2pwrite = os.pipe()
                    self.pid = os.fork()
                    if self.pid == 0:
                        # Child
                        os.dup2(p2cread, 0)
                        os.dup2(c2pwrite, 1)
                        os.dup2(c2pwrite, 2)
                        for i in range(3, popen2.MAXFD):
                            try:
                                os.close(i)
                            except: pass
                        try:
                            os.execvp(cmd[0], cmd)
                        finally:
                            os._exit(1)
                        # Shouldn't come here, I guess
                        os._exit(1)
                    os.close(p2cread)
                    self.tochild = os.fdopen(p2cwrite, 'w', bufsize)
                    os.close(c2pwrite)
                    self.fromchild = os.fdopen(c2pread, 'r', bufsize)
                    popen2._active.append(self)

            popen2.Popen4 = Popen4

        class Popen3(popen2.Popen3, popen2.Popen4):
            universal_newlines = 1
            def __init__(self, command, **kw):
                if kw.get('stderr') == 'STDOUT':
                    apply(popen2.Popen4.__init__, (self, command, 1))
                else:
                    apply(popen2.Popen3.__init__, (self, command, 1))
                self.stdin = self.tochild
                self.stdout = self.fromchild
                self.stderr = self.childerr
            def wait(self, *args, **kw):
                resultcode = apply(popen2.Popen3.wait, (self,)+args, kw)
                if os.WIFEXITED(resultcode):
                    return os.WEXITSTATUS(resultcode)
                elif os.WIFSIGNALED(resultcode):
                    return os.WTERMSIG(resultcode)
                else:
                    return None

    subprocess.Popen = Popen3



# From Josiah Carlson,
# ASPN : Python Cookbook : Module to allow Asynchronous subprocess use on Windows and Posix platforms
# http://aspn.activestate.com/ASPN/Cookbook/Python/Recipe/440554

PIPE = subprocess.PIPE

if subprocess.mswindows:
    from win32file import ReadFile, WriteFile
    from win32pipe import PeekNamedPipe
    import msvcrt
else:
    import select
    import fcntl

    try:                    fcntl.F_GETFL
    except AttributeError:  fcntl.F_GETFL = 3

    try:                    fcntl.F_SETFL
    except AttributeError:  fcntl.F_SETFL = 4

class Popen(subprocess.Popen):
    def recv(self, maxsize=None):
        return self._recv('stdout', maxsize)

    def recv_err(self, maxsize=None):
        return self._recv('stderr', maxsize)

    def send_recv(self, input='', maxsize=None):
        return self.send(input), self.recv(maxsize), self.recv_err(maxsize)

    def get_conn_maxsize(self, which, maxsize):
        if maxsize is None:
            maxsize = 1024
        elif maxsize < 1:
            maxsize = 1
        return getattr(self, which), maxsize

    def _close(self, which):
        getattr(self, which).close()
        setattr(self, which, None)

    if subprocess.mswindows:
        def send(self, input):
            if not self.stdin:
                return None

            try:
                x = msvcrt.get_osfhandle(self.stdin.fileno())
                (errCode, written) = WriteFile(x, input)
            except ValueError:
                return self._close('stdin')
            except (subprocess.pywintypes.error, Exception), why:
                if why[0] in (109, errno.ESHUTDOWN):
                    return self._close('stdin')
                raise

            return written

        def _recv(self, which, maxsize):
            conn, maxsize = self.get_conn_maxsize(which, maxsize)
            if conn is None:
                return None

            try:
                x = msvcrt.get_osfhandle(conn.fileno())
                (read, nAvail, nMessage) = PeekNamedPipe(x, 0)
                if maxsize < nAvail:
                    nAvail = maxsize
                if nAvail > 0:
                    (errCode, read) = ReadFile(x, nAvail, None)
            except ValueError:
                return self._close(which)
            except (subprocess.pywintypes.error, Exception), why:
                if why[0] in (109, errno.ESHUTDOWN):
                    return self._close(which)
                raise

            #if self.universal_newlines:
            #    read = self._translate_newlines(read)
            return read

    else:
        def send(self, input):
            if not self.stdin:
                return None

            if not select.select([], [self.stdin], [], 0)[1]:
                return 0

            try:
                written = os.write(self.stdin.fileno(), input)
            except OSError, why:
                if why[0] == errno.EPIPE: #broken pipe
                    return self._close('stdin')
                raise

            return written

        def _recv(self, which, maxsize):
            conn, maxsize = self.get_conn_maxsize(which, maxsize)
            if conn is None:
                return None

            try:
                flags = fcntl.fcntl(conn, fcntl.F_GETFL)
            except TypeError:
                flags = None
            else:
                if not conn.closed:
                    fcntl.fcntl(conn, fcntl.F_SETFL, flags| os.O_NONBLOCK)

            try:
                if not select.select([conn], [], [], 0)[0]:
                    return ''

                r = conn.read(maxsize)
                if not r:
                    return self._close(which)

                #if self.universal_newlines:
                #    r = self._translate_newlines(r)
                return r
            finally:
                if not conn.closed and not flags is None:
                    fcntl.fcntl(conn, fcntl.F_SETFL, flags)

disconnect_message = "Other end disconnected!"

def recv_some(p, t=.1, e=1, tr=5, stderr=0):
    if tr < 1:
        tr = 1
    x = time.time()+t
    y = []
    r = ''
    pr = p.recv
    if stderr:
        pr = p.recv_err
    while time.time() < x or r:
        r = pr()
        if r is None:
            if e:
                raise Exception(disconnect_message)
            else:
                break
        elif r:
            y.append(r)
        else:
            time.sleep(max((x-time.time())/tr, 0))
    return ''.join(y)

# TODO(3.0:  rewrite to use memoryview()
def send_all(p, data):
    while len(data):
        sent = p.send(data)
        if sent is None:
            raise Exception(disconnect_message)
        data = buffer(data, sent)



try:
    object
except NameError:
    class object:
        pass



class TestCmd(object):
    """Class TestCmd
    """

    def __init__(self, description = None,
                       program = None,
                       interpreter = None,
                       workdir = None,
                       subdir = None,
                       verbose = None,
                       match = None,
                       diff = None,
                       combine = 0,
                       universal_newlines = 1):
        self._cwd = os.getcwd()
        self.description_set(description)
        self.program_set(program)
        self.interpreter_set(interpreter)
        if verbose is None:
            try:
                verbose = max( 0, int(os.environ.get('TESTCMD_VERBOSE', 0)) )
            except ValueError:
                verbose = 0
        self.verbose_set(verbose)
        self.combine = combine
        self.universal_newlines = universal_newlines
        if match is not None:
            self.match_function = match
        else:
            self.match_function = match_re
        if diff is not None:
            self.diff_function = diff
        else:
            try:
                difflib
            except NameError:
                pass
            else:
                self.diff_function = simple_diff
                #self.diff_function = difflib.context_diff
                #self.diff_function = difflib.unified_diff
        self._dirlist = []
        self._preserve = {'pass_test': 0, 'fail_test': 0, 'no_result': 0}
        if os.environ.has_key('PRESERVE') and not os.environ['PRESERVE'] is '':
            self._preserve['pass_test'] = os.environ['PRESERVE']
            self._preserve['fail_test'] = os.environ['PRESERVE']
            self._preserve['no_result'] = os.environ['PRESERVE']
        else:
            try:
                self._preserve['pass_test'] = os.environ['PRESERVE_PASS']
            except KeyError:
                pass
            try:
                self._preserve['fail_test'] = os.environ['PRESERVE_FAIL']
            except KeyError:
                pass
            try:
                self._preserve['no_result'] = os.environ['PRESERVE_NO_RESULT']
            except KeyError:
                pass
        self._stdout = []
        self._stderr = []
        self.status = None
        self.condition = 'no_result'
        self.workdir_set(workdir)
        self.subdir(subdir)

    def __del__(self):
        self.cleanup()

    def __repr__(self):
        return "%x" % id(self)

    banner_char = '='
    banner_width = 80

    def banner(self, s, width=None):
        if width is None:
            width = self.banner_width
        return s + self.banner_char * (width - len(s))

    if os.name == 'posix':

        def escape(self, arg):
            "escape shell special characters"
            slash = '\\'
            special = '"$'

            arg = string.replace(arg, slash, slash+slash)
            for c in special:
                arg = string.replace(arg, c, slash+c)

            if re_space.search(arg):
                arg = '"' + arg + '"'
            return arg

    else:

        # Windows does not allow special characters in file names
        # anyway, so no need for an escape function, we will just quote
        # the arg.
        def escape(self, arg):
            if re_space.search(arg):
                arg = '"' + arg + '"'
            return arg

    def canonicalize(self, path):
        if is_List(path):
            path = apply(os.path.join, tuple(path))
        if not os.path.isabs(path):
            path = os.path.join(self.workdir, path)
        return path

    def chmod(self, path, mode):
        """Changes permissions on the specified file or directory
        path name."""
        path = self.canonicalize(path)
        os.chmod(path, mode)

    def cleanup(self, condition = None):
        """Removes any temporary working directories for the specified
        TestCmd environment.  If the environment variable PRESERVE was
        set when the TestCmd environment was created, temporary working
        directories are not removed.  If any of the environment variables
        PRESERVE_PASS, PRESERVE_FAIL, or PRESERVE_NO_RESULT were set
        when the TestCmd environment was created, then temporary working
        directories are not removed if the test passed, failed, or had
        no result, respectively.  Temporary working directories are also
        preserved for conditions specified via the preserve method.

        Typically, this method is not called directly, but is used when
        the script exits to clean up temporary working directories as
        appropriate for the exit status.
        """
        if not self._dirlist:
            return
        os.chdir(self._cwd)
        self.workdir = None
        if condition is None:
            condition = self.condition
        if self._preserve[condition]:
            for dir in self._dirlist:
                print "Preserved directory", dir
        else:
            list = self._dirlist[:]
            list.reverse()
            for dir in list:
                self.writable(dir, 1)
                shutil.rmtree(dir, ignore_errors = 1)
            self._dirlist = []

        try:
            global _Cleanup
            _Cleanup.remove(self)
        except (AttributeError, ValueError):
            pass

    def command_args(self, program = None,
                           interpreter = None,
                           arguments = None):
        if program:
            if type(program) == type('') and not os.path.isabs(program):
                program = os.path.join(self._cwd, program)
        else:
            program = self.program
            if not interpreter:
                interpreter = self.interpreter
        if not type(program) in [type([]), type(())]:
            program = [program]
        cmd = list(program)
        if interpreter:
            if not type(interpreter) in [type([]), type(())]:
                interpreter = [interpreter]
            cmd = list(interpreter) + cmd
        if arguments:
            if type(arguments) == type(''):
                arguments = string.split(arguments)
            cmd.extend(arguments)
        return cmd

    def description_set(self, description):
        """Set the description of the functionality being tested.
        """
        self.description = description

    try:
        difflib
    except NameError:
        def diff(self, a, b, name, *args, **kw):
            print self.banner('Expected %s' % name)
            print a
            print self.banner('Actual %s' % name)
            print b
    else:
        def diff(self, a, b, name, *args, **kw):
            print self.banner(name)
            args = (a.splitlines(), b.splitlines()) + args
            lines = apply(self.diff_function, args, kw)
            for l in lines:
                print l

    def fail_test(self, condition = 1, function = None, skip = 0):
        """Cause the test to fail.
        """
        if not condition:
            return
        self.condition = 'fail_test'
        fail_test(self = self,
                  condition = condition,
                  function = function,
                  skip = skip)

    def interpreter_set(self, interpreter):
        """Set the program to be used to interpret the program
        under test as a script.
        """
        self.interpreter = interpreter

    def match(self, lines, matches):
        """Compare actual and expected file contents.
        """
        return self.match_function(lines, matches)

    def match_exact(self, lines, matches):
        """Compare actual and expected file contents.
        """
        return match_exact(lines, matches)

    def match_re(self, lines, res):
        """Compare actual and expected file contents.
        """
        return match_re(lines, res)

    def match_re_dotall(self, lines, res):
        """Compare actual and expected file contents.
        """
        return match_re_dotall(lines, res)

    def no_result(self, condition = 1, function = None, skip = 0):
        """Report that the test could not be run.
        """
        if not condition:
            return
        self.condition = 'no_result'
        no_result(self = self,
                  condition = condition,
                  function = function,
                  skip = skip)

    def pass_test(self, condition = 1, function = None):
        """Cause the test to pass.
        """
        if not condition:
            return
        self.condition = 'pass_test'
        pass_test(self = self, condition = condition, function = function)

    def preserve(self, *conditions):
        """Arrange for the temporary working directories for the
        specified TestCmd environment to be preserved for one or more
        conditions.  If no conditions are specified, arranges for
        the temporary working directories to be preserved for all
        conditions.
        """
        if conditions is ():
            conditions = ('pass_test', 'fail_test', 'no_result')
        for cond in conditions:
            self._preserve[cond] = 1

    def program_set(self, program):
        """Set the executable program or script to be tested.
        """
        if program and not os.path.isabs(program):
            program = os.path.join(self._cwd, program)
        self.program = program

    def read(self, file, mode = 'rb'):
        """Reads and returns the contents of the specified file name.
        The file name may be a list, in which case the elements are
        concatenated with the os.path.join() method.  The file is
        assumed to be under the temporary working directory unless it
        is an absolute path name.  The I/O mode for the file may
        be specified; it must begin with an 'r'.  The default is
        'rb' (binary read).
        """
        file = self.canonicalize(file)
        if mode[0] != 'r':
            raise ValueError, "mode must begin with 'r'"
        with open(file, mode) as f:
            result = f.read()
        return result

    def rmdir(self, dir):
        """Removes the specified dir name.
        The dir name may be a list, in which case the elements are
        concatenated with the os.path.join() method.  The dir is
        assumed to be under the temporary working directory unless it
        is an absolute path name.
        The dir must be empty.
        """
        dir = self.canonicalize(dir)
        os.rmdir(dir)

    def start(self, program = None,
                    interpreter = None,
                    arguments = None,
                    universal_newlines = None,
                    **kw):
        """
        Starts a program or script for the test environment.

        The specified program will have the original directory
        prepended unless it is enclosed in a [list].
        """
        cmd = self.command_args(program, interpreter, arguments)
        cmd_string = string.join(map(self.escape, cmd), ' ')
        if self.verbose:
            sys.stderr.write(cmd_string + "\n")
        if universal_newlines is None:
            universal_newlines = self.universal_newlines

        # On Windows, if we make stdin a pipe when we plan to send 
        # no input, and the test program exits before
        # Popen calls msvcrt.open_osfhandle, that call will fail.
        # So don't use a pipe for stdin if we don't need one.
        stdin = kw.get('stdin', None)
        if stdin is not None:
            stdin = subprocess.PIPE

        combine = kw.get('combine', self.combine)
        if combine:
            stderr_value = subprocess.STDOUT
        else:
            stderr_value = subprocess.PIPE

        return Popen(cmd,
                     stdin=stdin,
                     stdout=subprocess.PIPE,
                     stderr=stderr_value,
                     universal_newlines=universal_newlines)

    def finish(self, popen, **kw):
        """
        Finishes and waits for the process being run under control of
        the specified popen argument, recording the exit status,
        standard output and error output.
        """
        popen.stdin.close()
        self.status = popen.wait()
        if not self.status:
            self.status = 0
        self._stdout.append(popen.stdout.read())
        if popen.stderr:
            stderr = popen.stderr.read()
        else:
            stderr = ''
        self._stderr.append(stderr)

    def run(self, program = None,
                  interpreter = None,
                  arguments = None,
                  chdir = None,
                  stdin = None,
                  universal_newlines = None):
        """Runs a test of the program or script for the test
        environment.  Standard output and error output are saved for
        future retrieval via the stdout() and stderr() methods.

        The specified program will have the original directory
        prepended unless it is enclosed in a [list].
        """
        if chdir:
            oldcwd = os.getcwd()
            if not os.path.isabs(chdir):
                chdir = os.path.join(self.workpath(chdir))
            if self.verbose:
                sys.stderr.write("chdir(" + chdir + ")\n")
            os.chdir(chdir)
        p = self.start(program,
                       interpreter,
                       arguments,
                       universal_newlines,
                       stdin=stdin)
        if stdin:
            if is_List(stdin):
                for line in stdin:
                    p.stdin.write(line)
            else:
                p.stdin.write(stdin)
            p.stdin.close()

        out = p.stdout.read()
        if p.stderr is None:
            err = ''
        else:
            err = p.stderr.read()
        try:
            close_output = p.close_output
        except AttributeError:
            p.stdout.close()
            if not p.stderr is None:
                p.stderr.close()
        else:
            close_output()

        self._stdout.append(out)
        self._stderr.append(err)

        self.status = p.wait()
        if not self.status:
            self.status = 0

        if chdir:
            os.chdir(oldcwd)
        if self.verbose >= 2:
            write = sys.stdout.write
            write('============ STATUS: %d\n' % self.status)
            out = self.stdout()
            if out or self.verbose >= 3:
                write('============ BEGIN STDOUT (len=%d):\n' % len(out))
                write(out)
                write('============ END STDOUT\n')
            err = self.stderr()
            if err or self.verbose >= 3:
                write('============ BEGIN STDERR (len=%d)\n' % len(err))
                write(err)
                write('============ END STDERR\n')

    def sleep(self, seconds = default_sleep_seconds):
        """Sleeps at least the specified number of seconds.  If no
        number is specified, sleeps at least the minimum number of
        seconds necessary to advance file time stamps on the current
        system.  Sleeping more seconds is all right.
        """
        time.sleep(seconds)

    def stderr(self, run = None):
        """Returns the error output from the specified run number.
        If there is no specified run number, then returns the error
        output of the last run.  If the run number is less than zero,
        then returns the error output from that many runs back from the
        current run.
        """
        if not run:
            run = len(self._stderr)
        elif run < 0:
            run = len(self._stderr) + run
        run = run - 1
        return self._stderr[run]

    def stdout(self, run = None):
        """Returns the standard output from the specified run number.
        If there is no specified run number, then returns the standard
        output of the last run.  If the run number is less than zero,
        then returns the standard output from that many runs back from
        the current run.
        """
        if not run:
            run = len(self._stdout)
        elif run < 0:
            run = len(self._stdout) + run
        run = run - 1
        return self._stdout[run]

    def subdir(self, *subdirs):
        """Create new subdirectories under the temporary working
        directory, one for each argument.  An argument may be a list,
        in which case the list elements are concatenated using the
        os.path.join() method.  Subdirectories multiple levels deep
        must be created using a separate argument for each level:

                test.subdir('sub', ['sub', 'dir'], ['sub', 'dir', 'ectory'])

        Returns the number of subdirectories actually created.
        """
        count = 0
        for sub in subdirs:
            if sub is None:
                continue
            if is_List(sub):
                sub = apply(os.path.join, tuple(sub))
            new = os.path.join(self.workdir, sub)
            try:
                os.mkdir(new)
            except OSError:
                pass
            else:
                count = count + 1
        return count

    def symlink(self, target, link):
        """Creates a symlink to the specified target.
        The link name may be a list, in which case the elements are
        concatenated with the os.path.join() method.  The link is
        assumed to be under the temporary working directory unless it
        is an absolute path name. The target is *not* assumed to be
        under the temporary working directory.
        """
        link = self.canonicalize(link)
        os.symlink(target, link)

    def tempdir(self, path=None):
        """Creates a temporary directory.
        A unique directory name is generated if no path name is specified.
        The directory is created, and will be removed when the TestCmd
        object is destroyed.
        """
        if path is None:
            try:
                path = tempfile.mktemp(prefix=tempfile.template)
            except TypeError:
                path = tempfile.mktemp()
        os.mkdir(path)

        # Symlinks in the path will report things
        # differently from os.getcwd(), so chdir there
        # and back to fetch the canonical path.
        cwd = os.getcwd()
        try:
            os.chdir(path)
            path = os.getcwd()
        finally:
            os.chdir(cwd)

        # Uppercase the drive letter since the case of drive
        # letters is pretty much random on win32:
        drive,rest = os.path.splitdrive(path)
        if drive:
            path = string.upper(drive) + rest

        #
        self._dirlist.append(path)
        global _Cleanup
        try:
            _Cleanup.index(self)
        except ValueError:
            _Cleanup.append(self)

        return path

    def touch(self, path, mtime=None):
        """Updates the modification time on the specified file or
        directory path name.  The default is to update to the
        current time if no explicit modification time is specified.
        """
        path = self.canonicalize(path)
        atime = os.path.getatime(path)
        if mtime is None:
            mtime = time.time()
        os.utime(path, (atime, mtime))

    def unlink(self, file):
        """Unlinks the specified file name.
        The file name may be a list, in which case the elements are
        concatenated with the os.path.join() method.  The file is
        assumed to be under the temporary working directory unless it
        is an absolute path name.
        """
        file = self.canonicalize(file)
        os.unlink(file)

    def verbose_set(self, verbose):
        """Set the verbose level.
        """
        self.verbose = verbose

    def where_is(self, file, path=None, pathext=None):
        """Find an executable file.
        """
        if is_List(file):
            file = apply(os.path.join, tuple(file))
        if not os.path.isabs(file):
            file = where_is(file, path, pathext)
        return file

    def workdir_set(self, path):
        """Creates a temporary working directory with the specified
        path name.  If the path is a null string (''), a unique
        directory name is created.
        """
        if (path != None):
            if path == '':
                path = None
            path = self.tempdir(path)
        self.workdir = path

    def workpath(self, *args):
        """Returns the absolute path name to a subdirectory or file
        within the current temporary working directory.  Concatenates
        the temporary working directory name with the specified
        arguments using the os.path.join() method.
        """
        return apply(os.path.join, (self.workdir,) + tuple(args))

    def readable(self, top, read=1):
        """Make the specified directory tree readable (read == 1)
        or not (read == None).

        This method has no effect on Windows systems, which use a
        completely different mechanism to control file readability.
        """

        if sys.platform == 'win32':
            return

        if read:
            def do_chmod(fname):
                try: st = os.stat(fname)
                except OSError: pass
                else: os.chmod(fname, stat.S_IMODE(st[stat.ST_MODE]|stat.S_IREAD))
        else:
            def do_chmod(fname):
                try: st = os.stat(fname)
                except OSError: pass
                else: os.chmod(fname, stat.S_IMODE(st[stat.ST_MODE]&~stat.S_IREAD))

        if os.path.isfile(top):
            # If it's a file, that's easy, just chmod it.
            do_chmod(top)
        elif read:
            # It's a directory and we're trying to turn on read
            # permission, so it's also pretty easy, just chmod the
            # directory and then chmod every entry on our walk down the
            # tree.  Because os.path.walk() is top-down, we'll enable
            # read permission on any directories that have it disabled
            # before os.path.walk() tries to list their contents.
            do_chmod(top)

            def chmod_entries(arg, dirname, names, do_chmod=do_chmod):
                for n in names:
                    do_chmod(os.path.join(dirname, n))

            os.path.walk(top, chmod_entries, None)
        else:
            # It's a directory and we're trying to turn off read
            # permission, which means we have to chmod the directoreis
            # in the tree bottom-up, lest disabling read permission from
            # the top down get in the way of being able to get at lower
            # parts of the tree.  But os.path.walk() visits things top
            # down, so we just use an object to collect a list of all
            # of the entries in the tree, reverse the list, and then
            # chmod the reversed (bottom-up) list.
            col = Collector(top)
            os.path.walk(top, col, None)
            col.entries.reverse()
            for d in col.entries: do_chmod(d)

    def writable(self, top, write=1):
        """Make the specified directory tree writable (write == 1)
        or not (write == None).
        """

        if sys.platform == 'win32':

            if write:
                def do_chmod(fname):
                    try: os.chmod(fname, stat.S_IWRITE)
                    except OSError: pass
            else:
                def do_chmod(fname):
                    try: os.chmod(fname, stat.S_IREAD)
                    except OSError: pass

        else:

            if write:
                def do_chmod(fname):
                    try: st = os.stat(fname)
                    except OSError: pass
                    else: os.chmod(fname, stat.S_IMODE(st[stat.ST_MODE]|0200))
            else:
                def do_chmod(fname):
                    try: st = os.stat(fname)
                    except OSError: pass
                    else: os.chmod(fname, stat.S_IMODE(st[stat.ST_MODE]&~0200))

        if os.path.isfile(top):
            do_chmod(top)
        else:
            col = Collector(top)
            os.path.walk(top, col, None)
            for d in col.entries: do_chmod(d)

    def executable(self, top, execute=1):
        """Make the specified directory tree executable (execute == 1)
        or not (execute == None).

        This method has no effect on Windows systems, which use a
        completely different mechanism to control file executability.
        """

        if sys.platform == 'win32':
            return

        if execute:
            def do_chmod(fname):
                try: st = os.stat(fname)
                except OSError: pass
                else: os.chmod(fname, stat.S_IMODE(st[stat.ST_MODE]|stat.S_IEXEC))
        else:
            def do_chmod(fname):
                try: st = os.stat(fname)
                except OSError: pass
                else: os.chmod(fname, stat.S_IMODE(st[stat.ST_MODE]&~stat.S_IEXEC))

        if os.path.isfile(top):
            # If it's a file, that's easy, just chmod it.
            do_chmod(top)
        elif execute:
            # It's a directory and we're trying to turn on execute
            # permission, so it's also pretty easy, just chmod the
            # directory and then chmod every entry on our walk down the
            # tree.  Because os.path.walk() is top-down, we'll enable
            # execute permission on any directories that have it disabled
            # before os.path.walk() tries to list their contents.
            do_chmod(top)

            def chmod_entries(arg, dirname, names, do_chmod=do_chmod):
                for n in names:
                    do_chmod(os.path.join(dirname, n))

            os.path.walk(top, chmod_entries, None)
        else:
            # It's a directory and we're trying to turn off execute
            # permission, which means we have to chmod the directories
            # in the tree bottom-up, lest disabling execute permission from
            # the top down get in the way of being able to get at lower
            # parts of the tree.  But os.path.walk() visits things top
            # down, so we just use an object to collect a list of all
            # of the entries in the tree, reverse the list, and then
            # chmod the reversed (bottom-up) list.
            col = Collector(top)
            os.path.walk(top, col, None)
            col.entries.reverse()
            for d in col.entries: do_chmod(d)

    def write(self, file, content, mode = 'wb'):
        """Writes the specified content text (second argument) to the
        specified file name (first argument).  The file name may be
        a list, in which case the elements are concatenated with the
        os.path.join() method.  The file is created under the temporary
        working directory.  Any subdirectories in the path must already
        exist.  The I/O mode for the file may be specified; it must
        begin with a 'w'.  The default is 'wb' (binary write).
        """
        file = self.canonicalize(file)
        if mode[0] != 'w':
            raise ValueError, "mode must begin with 'w'"
        with open(file, mode) as f:
            f.write(content)

# Local Variables:
# tab-width:4
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=4 shiftwidth=4:
