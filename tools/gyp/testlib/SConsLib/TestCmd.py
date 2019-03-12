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
                           match_stdout = default_match_stdout_function,
                           match_stderr = default_match_stderr_function,
                           diff = default_diff_stderr_function,
                           diff_stdout = default_diff_stdout_function,
                           diff_stderr = default_diff_stderr_function,
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
    test.fail_test(condition, function, skip, message)

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

    test.diff_stderr(actual, expected)

    test.diff_stdout(actual, expected)

    test.match(actual, expected)

    test.match_stderr(actual, expected)

    test.match_stdout(actual, expected)

    test.set_match_function(match, stdout, stderr)

    test.match_exact("actual 1\nactual 2\n", "expected 1\nexpected 2\n")
    test.match_exact(["actual 1\n", "actual 2\n"],
                     ["expected 1\n", "expected 2\n"])
    test.match_caseinsensitive("Actual 1\nACTUAL 2\n", "expected 1\nEXPECTED 2\n")

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
    TestCmd.fail_test(condition, function, skip, message)

    TestCmd.no_result()
    TestCmd.no_result(condition)
    TestCmd.no_result(condition, function)
    TestCmd.no_result(condition, function, skip)

The TestCmd module also provides unbound global functions that handle
matching in the same way as the match_*() methods described above.

    import TestCmd

    test = TestCmd.TestCmd(match = TestCmd.match_exact)

    test = TestCmd.TestCmd(match = TestCmd.match_caseinsensitive)

    test = TestCmd.TestCmd(match = TestCmd.match_re)

    test = TestCmd.TestCmd(match = TestCmd.match_re_dotall)

These functions are also available as static methods:

    import TestCmd

    test = TestCmd.TestCmd(match = TestCmd.TestCmd.match_exact)

    test = TestCmd.TestCmd(match = TestCmd.TestCmd.match_caseinsensitive)

    test = TestCmd.TestCmd(match = TestCmd.TestCmd.match_re)

    test = TestCmd.TestCmd(match = TestCmd.TestCmd.match_re_dotall)

These static methods can be accessed by a string naming the method:

    import TestCmd

    test = TestCmd.TestCmd(match = 'match_exact')

    test = TestCmd.TestCmd(match = 'match_caseinsensitive')

    test = TestCmd.TestCmd(match = 'match_re')

    test = TestCmd.TestCmd(match = 'match_re_dotall')

The TestCmd module provides unbound global functions that can be used
for the "diff" argument to TestCmd.TestCmd instantiation:

    import TestCmd

    test = TestCmd.TestCmd(match = TestCmd.match_re,
                           diff = TestCmd.diff_re)

    test = TestCmd.TestCmd(diff = TestCmd.simple_diff)

    test = TestCmd.TestCmd(diff = TestCmd.context_diff)

    test = TestCmd.TestCmd(diff = TestCmd.unified_diff)

These functions are also available as static methods:

    import TestCmd

    test = TestCmd.TestCmd(match = TestCmd.TestCmd.match_re,
                           diff = TestCmd.TestCmd.diff_re)

    test = TestCmd.TestCmd(diff = TestCmd.TestCmd.simple_diff)

    test = TestCmd.TestCmd(diff = TestCmd.TestCmd.context_diff)

    test = TestCmd.TestCmd(diff = TestCmd.TestCmd.unified_diff)

These static methods can be accessed by a string naming the method:

    import TestCmd

    test = TestCmd.TestCmd(match = 'match_re', diff = 'diff_re')

    test = TestCmd.TestCmd(diff = 'simple_diff')

    test = TestCmd.TestCmd(diff = 'context_diff')

    test = TestCmd.TestCmd(diff = 'unified_diff')

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
from __future__ import division, print_function

__author__ = "Steven Knight <knight at baldmt dot com>"
__revision__ = "TestCmd.py 1.3.D001 2010/06/03 12:58:27 knight"
__version__ = "1.3"

import atexit
import difflib
import errno
import os
import re
import shutil
import signal
import stat
import sys
import tempfile
import threading
import time
import traceback
import types


IS_PY3 = sys.version_info[0] == 3
IS_WINDOWS = sys.platform == 'win32'
IS_64_BIT = sys.maxsize > 2**32

class null(object):
    pass


_Null = null()

try:
    from collections import UserList, UserString
except ImportError:
    # no 'collections' module or no UserFoo in collections
    exec('from UserList import UserList')
    exec('from UserString import UserString')

__all__ = [
    'diff_re',
    'fail_test',
    'no_result',
    'pass_test',
    'match_exact',
    'match_caseinsensitive',
    'match_re',
    'match_re_dotall',
    'python',
    '_python_',
    'TestCmd',
    'to_bytes',
    'to_str',
]


def is_List(e):
    return isinstance(e, (list, UserList))


def to_bytes(s):
    if isinstance(s, bytes) or bytes is str:
        return s
    return bytes(s, 'utf-8')


def to_str(s):
    if bytes is str or is_String(s):
        return s
    return str(s, 'utf-8')


try:
    eval('unicode')
except NameError:
    def is_String(e):
        return isinstance(e, (str, UserString))
else:
    def is_String(e):
        return isinstance(e, (str, unicode, UserString))

tempfile.template = 'testcmd.'
if os.name in ('posix', 'nt'):
    tempfile.template = 'testcmd.' + str(os.getpid()) + '.'
else:
    tempfile.template = 'testcmd.'

re_space = re.compile('\s')


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


def fail_test(self=None, condition=1, function=None, skip=0, message=None):
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
    if message:
        msg = "\t%s\n" % message
    else:
        msg = ""
    sys.stderr.write("FAILED test" + of + desc + sep + at + msg)

    sys.exit(1)


def no_result(self=None, condition=1, function=None, skip=0):
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

    at = _caller(traceback.extract_stack(), skip)
    sys.stderr.write("NO RESULT for test" + of + desc + sep + at)

    sys.exit(2)


def pass_test(self=None, condition=1, function=None):
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


def match_exact(lines=None, matches=None, newline=os.sep):
    """
    """

    if isinstance(lines, bytes) or bytes is str:
        newline = to_bytes(newline)

    if not is_List(lines):
        lines = lines.split(newline)
    if not is_List(matches):
        matches = matches.split(newline)
    if len(lines) != len(matches):
        return
    for i in range(len(lines)):
        if lines[i] != matches[i]:
            return
    return 1


def match_caseinsensitive(lines=None, matches=None):
    """
    """
    if not is_List(lines):
        lines = lines.split("\n")
    if not is_List(matches):
        matches = matches.split("\n")
    if len(lines) != len(matches):
        return
    for i in range(len(lines)):
        if lines[i].lower() != matches[i].lower():
            return
    return 1


def match_re(lines=None, res=None):
    """
    """
    if not is_List(lines):
        # CRs mess up matching (Windows) so split carefully
        lines = re.split('\r?\n', lines)
    if not is_List(res):
        res = res.split("\n")
    if len(lines) != len(res):
        print("match_re: expected %d lines, found %d" % (len(res), len(lines)))
        return
    for i in range(len(lines)):
        s = "^" + res[i] + "$"
        try:
            expr = re.compile(s)
        except re.error as e:
            msg = "Regular expression error in %s: %s"
            raise re.error(msg % (repr(s), e.args[0]))
        if not expr.search(lines[i]):
            print("match_re: mismatch at line %d:\n  search re='%s'\n  line='%s'" % (
                i, s, lines[i]))
            return
    return 1


def match_re_dotall(lines=None, res=None):
    """
    """
    if not isinstance(lines, str):
        lines = "\n".join(lines)
    if not isinstance(res, str):
        res = "\n".join(res)
    s = "^" + res + "$"
    try:
        expr = re.compile(s, re.DOTALL)
    except re.error as e:
        msg = "Regular expression error in %s: %s"
        raise re.error(msg % (repr(s), e.args[0]))
    return expr.match(lines)


def simple_diff(a, b, fromfile='', tofile='',
                fromfiledate='', tofiledate='', n=3, lineterm='\n'):
    """
    A function with the same calling signature as difflib.context_diff
    (diff -c) and difflib.unified_diff (diff -u) but which prints
    output like the simple, unadorned 'diff" command.
    """
    a = [to_str(q) for q in a]
    b = [to_str(q) for q in b]
    sm = difflib.SequenceMatcher(None, a, b)

    def comma(x1, x2):
        return x1 + 1 == x2 and str(x2) or '%s,%s' % (x1 + 1, x2)
    result = []
    for op, a1, a2, b1, b2 in sm.get_opcodes():
        if op == 'delete':
            result.append("%sd%d" % (comma(a1, a2), b1))
            result.extend(['< ' + l for l in a[a1:a2]])
        elif op == 'insert':
            result.append("%da%s" % (a1, comma(b1, b2)))
            result.extend(['> ' + l for l in b[b1:b2]])
        elif op == 'replace':
            result.append("%sc%s" % (comma(a1, a2), comma(b1, b2)))
            result.extend(['< ' + l for l in a[a1:a2]])
            result.append('---')
            result.extend(['> ' + l for l in b[b1:b2]])
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
        a = a + [''] * (-diff)
    elif diff > 0:
        b = b + [''] * diff
    i = 0
    for aline, bline in zip(a, b):
        s = "^" + aline + "$"
        try:
            expr = re.compile(s)
        except re.error as e:
            msg = "Regular expression error in %s: %s"
            raise re.error(msg % (repr(s), e.args[0]))
        if not expr.search(bline):
            result.append("%sc%s" % (i + 1, i + 1))
            result.append('< ' + repr(a[i]))
            result.append('---')
            result.append('> ' + repr(b[i]))
        i = i + 1
    return result


if os.name == 'posix':
    def escape(arg):
        "escape shell special characters"
        slash = '\\'
        special = '"$'
        arg = arg.replace(slash, slash + slash)
        for c in special:
            arg = arg.replace(c, slash + c)
        if re_space.search(arg):
            arg = '"' + arg + '"'
        return arg
else:
    # Windows does not allow special characters in file names
    # anyway, so no need for an escape function, we will just quote
    # the arg.
    def escape(arg):
        if re_space.search(arg):
            arg = '"' + arg + '"'
        return arg

if os.name == 'java':
    python = os.path.join(sys.prefix, 'jython')
else:
    python = os.environ.get('python_executable', sys.executable)
_python_ = escape(python)

if sys.platform == 'win32':

    default_sleep_seconds = 2

    def where_is(file, path=None, pathext=None):
        if path is None:
            path = os.environ['PATH']
        if is_String(path):
            path = path.split(os.pathsep)
        if pathext is None:
            pathext = os.environ['PATHEXT']
        if is_String(pathext):
            pathext = pathext.split(os.pathsep)
        for ext in pathext:
            if ext.lower() == file[-len(ext):].lower():
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
            path = path.split(os.pathsep)
        for dir in path:
            f = os.path.join(dir, file)
            if os.path.isfile(f):
                try:
                    st = os.stat(f)
                except OSError:
                    continue
                if stat.S_IMODE(st[stat.ST_MODE]) & 0o111:
                    return f
        return None

    default_sleep_seconds = 1


import subprocess

try:
    subprocess.Popen.terminate
except AttributeError:
    if sys.platform == 'win32':
        import win32process

        def terminate(self):
            win32process.TerminateProcess(self._handle, 1)
    else:
        def terminate(self):
            os.kill(self.pid, signal.SIGTERM)
    method = types.MethodType(terminate, None, subprocess.Popen)
    setattr(subprocess.Popen, 'terminate', method)


# From Josiah Carlson,
# ASPN : Python Cookbook : Module to allow Asynchronous subprocess use on Windows and Posix platforms
# http://aspn.activestate.com/ASPN/Cookbook/Python/Recipe/440554

PIPE = subprocess.PIPE

if sys.platform == 'win32':  # and subprocess.mswindows:
    try:
        from win32file import ReadFile, WriteFile
        from win32pipe import PeekNamedPipe
    except ImportError:
        # If PyWin32 is not available, try ctypes instead
        # XXX These replicate _just_enough_ PyWin32 behaviour for our purposes
        import ctypes
        from ctypes.wintypes import DWORD

        def ReadFile(hFile, bufSize, ol=None):
            assert ol is None
            lpBuffer = ctypes.create_string_buffer(bufSize)
            bytesRead = DWORD()
            bErr = ctypes.windll.kernel32.ReadFile(
                hFile, lpBuffer, bufSize, ctypes.byref(bytesRead), ol)
            if not bErr:
                raise ctypes.WinError()
            return (0, ctypes.string_at(lpBuffer, bytesRead.value))

        def WriteFile(hFile, data, ol=None):
            assert ol is None
            bytesWritten = DWORD()
            bErr = ctypes.windll.kernel32.WriteFile(
                hFile, data, len(data), ctypes.byref(bytesWritten), ol)
            if not bErr:
                raise ctypes.WinError()
            return (0, bytesWritten.value)

        def PeekNamedPipe(hPipe, size):
            assert size == 0
            bytesAvail = DWORD()
            bErr = ctypes.windll.kernel32.PeekNamedPipe(
                hPipe, None, size, None, ctypes.byref(bytesAvail), None)
            if not bErr:
                raise ctypes.WinError()
            return ("", bytesAvail.value, None)
    import msvcrt
else:
    import select
    import fcntl

    try:
        fcntl.F_GETFL
    except AttributeError:
        fcntl.F_GETFL = 3

    try:
        fcntl.F_SETFL
    except AttributeError:
        fcntl.F_SETFL = 4


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

    if sys.platform == 'win32':  # and subprocess.mswindows:
        def send(self, input):
            input = to_bytes(input)
            if not self.stdin:
                return None

            try:
                x = msvcrt.get_osfhandle(self.stdin.fileno())
                (errCode, written) = WriteFile(x, input)
            except ValueError:
                return self._close('stdin')
            except (subprocess.pywintypes.error, Exception) as why:
                if why.args[0] in (109, errno.ESHUTDOWN):
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
            except (subprocess.pywintypes.error, Exception) as why:
                if why.args[0] in (109, errno.ESHUTDOWN):
                    return self._close(which)
                raise

            # if self.universal_newlines:
            #    read = self._translate_newlines(read)
            return read

    else:
        def send(self, input):
            if not self.stdin:
                return None

            if not select.select([], [self.stdin], [], 0)[1]:
                return 0

            try:
                written = os.write(self.stdin.fileno(),
                                   bytearray(input, 'utf-8'))
            except OSError as why:
                if why.args[0] == errno.EPIPE:  # broken pipe
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
                    fcntl.fcntl(conn, fcntl.F_SETFL, flags | os.O_NONBLOCK)

            try:
                if not select.select([conn], [], [], 0)[0]:
                    return ''

                r = conn.read(maxsize)
                if not r:
                    return self._close(which)

                # if self.universal_newlines:
                #    r = self._translate_newlines(r)
                return r
            finally:
                if not conn.closed and not flags is None:
                    fcntl.fcntl(conn, fcntl.F_SETFL, flags)


disconnect_message = "Other end disconnected!"


def recv_some(p, t=.1, e=1, tr=5, stderr=0):
    if tr < 1:
        tr = 1
    x = time.time() + t
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
            time.sleep(max((x - time.time()) / tr, 0))
    return ''.join(y)


def send_all(p, data):
    while len(data):
        sent = p.send(data)
        if sent is None:
            raise Exception(disconnect_message)
        data = memoryview(data)[sent:]


_Cleanup = []


def _clean():
    global _Cleanup
    cleanlist = [c for c in _Cleanup if c]
    del _Cleanup[:]
    cleanlist.reverse()
    for test in cleanlist:
        test.cleanup()


atexit.register(_clean)


class TestCmd(object):
    """Class TestCmd
    """

    def __init__(self, description=None,
                 program=None,
                 interpreter=None,
                 workdir=None,
                 subdir=None,
                 verbose=None,
                 match=None,
                 match_stdout=None,
                 match_stderr=None,
                 diff=None,
                 diff_stdout=None,
                 diff_stderr=None,
                 combine=0,
                 universal_newlines=True,
                 timeout=None):
        self.external = os.environ.get('SCONS_EXTERNAL_TEST', 0)
        self._cwd = os.getcwd()
        self.description_set(description)
        self.program_set(program)
        self.interpreter_set(interpreter)
        if verbose is None:
            try:
                verbose = max(0, int(os.environ.get('TESTCMD_VERBOSE', 0)))
            except ValueError:
                verbose = 0
        self.verbose_set(verbose)
        self.combine = combine
        self.universal_newlines = universal_newlines
        self.process = None
        self.set_timeout(timeout)
        self.set_match_function(match, match_stdout, match_stderr)
        self.set_diff_function(diff, diff_stdout, diff_stderr)
        self._dirlist = []
        self._preserve = {'pass_test': 0, 'fail_test': 0, 'no_result': 0}
        preserve_value = os.environ.get('PRESERVE', False)
        if preserve_value not in [0, '0', 'False']:
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
        self.fixture_dirs = []

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

    escape = staticmethod(escape)

    def canonicalize(self, path):
        if is_List(path):
            path = os.path.join(*tuple(path))
        if not os.path.isabs(path):
            path = os.path.join(self.workdir, path)
        return path

    def chmod(self, path, mode):
        """Changes permissions on the specified file or directory
        path name."""
        path = self.canonicalize(path)
        os.chmod(path, mode)

    def cleanup(self, condition=None):
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
                print(u"Preserved directory " + dir + "\n")
        else:
            list = self._dirlist[:]
            list.reverse()
            for dir in list:
                self.writable(dir, 1)
                shutil.rmtree(dir, ignore_errors=1)
            self._dirlist = []

            global _Cleanup
            if self in _Cleanup:
                _Cleanup.remove(self)

    def command_args(self, program=None,
                     interpreter=None,
                     arguments=None):
        if not self.external:
            if program:
                if isinstance(program, str) and not os.path.isabs(program):
                    program = os.path.join(self._cwd, program)
            else:
                program = self.program
                if not interpreter:
                    interpreter = self.interpreter
        else:
            if not program:
                program = self.program
                if not interpreter:
                    interpreter = self.interpreter
        if not isinstance(program, (list, tuple)):
            program = [program]
        cmd = list(program)
        if interpreter:
            if not isinstance(interpreter, (list, tuple)):
                interpreter = [interpreter]
            cmd = list(interpreter) + cmd
        if arguments:
            if isinstance(arguments, str):
                arguments = arguments.split()
            cmd.extend(arguments)
        return cmd

    def description_set(self, description):
        """Set the description of the functionality being tested.
        """
        self.description = description

    def set_diff_function(self, diff=_Null, stdout=_Null, stderr=_Null):
        """Sets the specified diff functions.
        """
        if diff is not _Null:
            self._diff_function = diff
        if stdout is not _Null:
            self._diff_stdout_function = stdout
        if stderr is not _Null:
            self._diff_stderr_function = stderr

    def diff(self, a, b, name=None, diff_function=None, *args, **kw):
        if diff_function is None:
            try:
                diff_function = getattr(self, self._diff_function)
            except TypeError:
                diff_function = self._diff_function
                if diff_function is None:
                    diff_function = self.simple_diff
        if name is not None:
            print(self.banner(name))

        if not is_List(a):
            a=a.splitlines()
        if not is_List(b):
            b=b.splitlines()

        args = (a, b) + args
        for line in diff_function(*args, **kw):
            print(line)

    def diff_stderr(self, a, b, *args, **kw):
        """Compare actual and expected file contents.
        """
        try:
            diff_stderr_function = getattr(self, self._diff_stderr_function)
        except TypeError:
            diff_stderr_function = self._diff_stderr_function
        return self.diff(a, b, diff_function=diff_stderr_function, *args, **kw)

    def diff_stdout(self, a, b, *args, **kw):
        """Compare actual and expected file contents.
        """
        try:
            diff_stdout_function = getattr(self, self._diff_stdout_function)
        except TypeError:
            diff_stdout_function = self._diff_stdout_function
        return self.diff(a, b, diff_function=diff_stdout_function, *args, **kw)

    simple_diff = staticmethod(simple_diff)

    diff_re = staticmethod(diff_re)

    context_diff = staticmethod(difflib.context_diff)

    unified_diff = staticmethod(difflib.unified_diff)

    def fail_test(self, condition=1, function=None, skip=0, message=None):
        """Cause the test to fail.
        """
        if not condition:
            return
        self.condition = 'fail_test'
        fail_test(self=self,
                  condition=condition,
                  function=function,
                  skip=skip,
                  message=message)

    def interpreter_set(self, interpreter):
        """Set the program to be used to interpret the program
        under test as a script.
        """
        self.interpreter = interpreter

    def set_match_function(self, match=_Null, stdout=_Null, stderr=_Null):
        """Sets the specified match functions.
        """
        if match is not _Null:
            self._match_function = match
        if stdout is not _Null:
            self._match_stdout_function = stdout
        if stderr is not _Null:
            self._match_stderr_function = stderr

    def match(self, lines, matches):
        """Compare actual and expected file contents.
        """
        try:
            match_function = getattr(self, self._match_function)
        except TypeError:
            match_function = self._match_function
            if match_function is None:
                # Default is regular expression matches.
                match_function = self.match_re
        return match_function(lines, matches)

    def match_stderr(self, lines, matches):
        """Compare actual and expected file contents.
        """
        try:
            match_stderr_function = getattr(self, self._match_stderr_function)
        except TypeError:
            match_stderr_function = self._match_stderr_function
            if match_stderr_function is None:
                # Default is to use whatever match= is set to.
                match_stderr_function = self.match
        return match_stderr_function(lines, matches)

    def match_stdout(self, lines, matches):
        """Compare actual and expected file contents.
        """
        try:
            match_stdout_function = getattr(self, self._match_stdout_function)
        except TypeError:
            match_stdout_function = self._match_stdout_function
            if match_stdout_function is None:
                # Default is to use whatever match= is set to.
                match_stdout_function = self.match
        return match_stdout_function(lines, matches)

    match_exact = staticmethod(match_exact)

    match_caseinsensitive = staticmethod(match_caseinsensitive)

    match_re = staticmethod(match_re)

    match_re_dotall = staticmethod(match_re_dotall)

    def no_result(self, condition=1, function=None, skip=0):
        """Report that the test could not be run.
        """
        if not condition:
            return
        self.condition = 'no_result'
        no_result(self=self,
                  condition=condition,
                  function=function,
                  skip=skip)

    def pass_test(self, condition=1, function=None):
        """Cause the test to pass.
        """
        if not condition:
            return
        self.condition = 'pass_test'
        pass_test(self=self, condition=condition, function=function)

    def preserve(self, *conditions):
        """Arrange for the temporary working directories for the
        specified TestCmd environment to be preserved for one or more
        conditions.  If no conditions are specified, arranges for
        the temporary working directories to be preserved for all
        conditions.
        """
        if not conditions:
            conditions = ('pass_test', 'fail_test', 'no_result')
        for cond in conditions:
            self._preserve[cond] = 1

    def program_set(self, program):
        """Set the executable program or script to be tested.
        """
        if not self.external:
            if program and not os.path.isabs(program):
                program = os.path.join(self._cwd, program)
        self.program = program

    def read(self, file, mode='rb', newline=None):
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
            raise ValueError("mode must begin with 'r'")
        if IS_PY3 and 'b' not in mode:
            with open(file, mode, newline=newline) as f:
                return f.read()
        else:
            with open(file, mode) as f:
                return f.read()

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

    def _timeout(self):
        self.process.terminate()
        self.timer.cancel()
        self.timer = None

    def set_timeout(self, timeout):
        self.timeout = timeout
        self.timer = None

    def parse_path(self, path, suppress_current=False):
        """Return a list with the single path components of path.
        """
        head, tail = os.path.split(path)
        result = []
        if not tail:
            if head == path:
                return [head]
        else:
            result.append(tail)
        head, tail = os.path.split(head)
        while head and tail:
            result.append(tail)
            head, tail = os.path.split(head)
        result.append(head or tail)
        result.reverse()

        return result

    def dir_fixture(self, srcdir, dstdir=None):
        """Copies the contents of the specified folder srcdir from
        the directory of the called  script, to the current
        working directory.
        The srcdir name may be a list, in which case the elements are
        concatenated with the os.path.join() method.  The dstdir is
        assumed to be under the temporary working directory, it gets
        created automatically, if it does not already exist.
        """

        if srcdir and self.fixture_dirs and not os.path.isabs(srcdir):
            for dir in self.fixture_dirs:
                spath = os.path.join(dir, srcdir)
                if os.path.isdir(spath):
                    break
        else:
            spath = srcdir

        if dstdir:
            dstdir = self.canonicalize(dstdir)
        else:
            dstdir = '.'

        if dstdir != '.' and not os.path.exists(dstdir):
            dstlist = self.parse_path(dstdir)
            if len(dstlist) > 0 and dstlist[0] == ".":
                dstlist = dstlist[1:]
            for idx in range(len(dstlist)):
                self.subdir(dstlist[:idx + 1])

        if dstdir and self.workdir:
            dstdir = os.path.join(self.workdir, dstdir)

        for entry in os.listdir(spath):
            epath = os.path.join(spath, entry)
            dpath = os.path.join(dstdir, entry)
            if os.path.isdir(epath):
                # Copy the subfolder
                shutil.copytree(epath, dpath)
            else:
                shutil.copy(epath, dpath)

    def file_fixture(self, srcfile, dstfile=None):
        """Copies the file srcfile from the directory of
        the called script, to the current working directory.
        The dstfile is assumed to be under the temporary working
        directory unless it is an absolute path name.
        If dstfile is specified its target directory gets created
        automatically, if it does not already exist.
        """
        srcpath, srctail = os.path.split(srcfile)

        if srcpath and (not self.fixture_dirs or os.path.isabs(srcpath)):
            spath = srcfile
        else:
            for dir in self.fixture_dirs:
                spath = os.path.join(dir, srcfile)
                if os.path.isfile(spath):
                    break

        if not dstfile:
            if srctail:
                dpath = os.path.join(self.workdir, srctail)
            else:
                return
        else:
            dstpath, dsttail = os.path.split(dstfile)
            if dstpath:
                if not os.path.exists(os.path.join(self.workdir, dstpath)):
                    dstlist = self.parse_path(dstpath)
                    if len(dstlist) > 0 and dstlist[0] == ".":
                        dstlist = dstlist[1:]
                    for idx in range(len(dstlist)):
                        self.subdir(dstlist[:idx + 1])

            dpath = os.path.join(self.workdir, dstfile)
        shutil.copy(spath, dpath)

    def start(self, program=None,
              interpreter=None,
              arguments=None,
              universal_newlines=None,
              timeout=_Null,
              **kw):
        """
        Starts a program or script for the test environment.

        The specified program will have the original directory
        prepended unless it is enclosed in a [list].
        """
        cmd = self.command_args(program, interpreter, arguments)
        if self.verbose:
            cmd_string = ' '.join([self.escape(c) for c in cmd])
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

        if timeout is _Null:
            timeout = self.timeout
        if timeout:
            self.timer = threading.Timer(float(timeout), self._timeout)
            self.timer.start()

        if IS_PY3 and sys.platform == 'win32':
            # Set this otherwist stdout/stderr pipes default to
            # windows default locale cp1252 which will throw exception
            # if using non-ascii characters.
            # For example test/Install/non-ascii-name.py
            os.environ['PYTHONIOENCODING'] = 'utf-8'

        # It seems that all pythons up to py3.6 still set text mode if you set encoding.
        # TODO: File enhancement request on python to propagate universal_newlines even
        # if encoding is set.hg c
        p = Popen(cmd,
                  stdin=stdin,
                  stdout=subprocess.PIPE,
                  stderr=stderr_value,
                  env=os.environ,
                  universal_newlines=False)

        self.process = p
        return p

    @staticmethod
    def fix_binary_stream(stream):
        """
        Handle stdout/stderr from popen when we specify universal_newlines = False.
        This will read from the pipes in binary mode, not decode the output,
        and not convert line endings to \n.
        We do this because in py3 (3.5) with universal_newlines=True, it will
        choose the default system locale to decode the output, and this breaks unicode
        output. Specifically breaking test/option--tree.py which outputs a unicode char.

        py 3.6 allows us to pass an encoding param to popen thus not requiring the decode
        nor end of line handling, because we propagate universal_newlines as specified.

        TODO: Do we need to pass universal newlines into this function?
        """

        if not stream:
            return stream
        # TODO: Run full tests on both platforms and see if this fixes failures
        # It seems that py3.6 still sets text mode if you set encoding.
        elif sys.version_info[0] == 3:# TODO and sys.version_info[1] < 6:
            stream = stream.decode('utf-8')
            stream = stream.replace('\r\n', '\n')
        elif sys.version_info[0] == 2:
            stream = stream.replace('\r\n', '\n')

        return stream


    def finish(self, popen=None, **kw):
        """
        Finishes and waits for the process being run under control of
        the specified popen argument, recording the exit status,
        standard output and error output.
        """
        if popen is None:
            popen = self.process
        stdout, stderr = popen.communicate()

        stdout = self.fix_binary_stream(stdout)
        stderr = self.fix_binary_stream(stderr)

        if self.timer:
            self.timer.cancel()
            self.timer = None
        self.status = popen.returncode
        self.process = None
        self._stdout.append(stdout or '')
        self._stderr.append(stderr or '')

    def run(self, program=None,
            interpreter=None,
            arguments=None,
            chdir=None,
            stdin=None,
            universal_newlines=None,
            timeout=_Null):
        """Runs a test of the program or script for the test
        environment.  Standard output and error output are saved for
        future retrieval via the stdout() and stderr() methods.

        The specified program will have the original directory
        prepended unless it is enclosed in a [list].
        """
        if self.external:
            if not program:
                program = self.program
            if not interpreter:
                interpreter = self.interpreter

        if universal_newlines is None:
            universal_newlines = self.universal_newlines

        if chdir:
            oldcwd = os.getcwd()
            if not os.path.isabs(chdir):
                chdir = os.path.join(self.workpath(chdir))
            if self.verbose:
                sys.stderr.write("chdir(" + chdir + ")\n")
            os.chdir(chdir)
        p = self.start(program=program,
                       interpreter=interpreter,
                       arguments=arguments,
                       universal_newlines=universal_newlines,
                       timeout=timeout,
                       stdin=stdin)
        if is_List(stdin):
            stdin = ''.join(stdin)

        if stdin and IS_PY3:#  and sys.version_info[1] < 6:
            stdin = to_bytes(stdin)

        # TODO(sgk):  figure out how to re-use the logic in the .finish()
        # method above.  Just calling it from here causes problems with
        # subclasses that redefine .finish().  We could abstract this
        # into Yet Another common method called both here and by .finish(),
        # but that seems ill-thought-out.
        stdout, stderr = p.communicate(input=stdin)
        if self.timer:
            self.timer.cancel()
            self.timer = None
        self.status = p.returncode
        self.process = None

        stdout = self.fix_binary_stream(stdout)
        stderr = self.fix_binary_stream(stderr)


        self._stdout.append(stdout or '')
        self._stderr.append(stderr or '')

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

    def sleep(self, seconds=default_sleep_seconds):
        """Sleeps at least the specified number of seconds.  If no
        number is specified, sleeps at least the minimum number of
        seconds necessary to advance file time stamps on the current
        system.  Sleeping more seconds is all right.
        """
        time.sleep(seconds)

    def stderr(self, run=None):
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
        if run >= 0 and len(self._stderr) > run:
          return self._stderr[run]
        else:
          ''

    def stdout(self, run=None):
        """
        Returns the stored standard output from a given run.

        Args:
            run: run number to select.  If run number is omitted,
            return the standard output of the most recent run.
            If negative, use as a relative offset, so that -2
            means the run two prior to the most recent.

        Returns:
            selected stdout string or None if there are no
            stored runs.
        """
        if not run:
            run = len(self._stdout)
        elif run < 0:
            run = len(self._stdout) + run
        run = run - 1
        if run >= 0 and len(self._stdout) > run:
            return self._stdout[run]
        else:
          ''

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
                sub = os.path.join(*tuple(sub))
            new = os.path.join(self.workdir, sub)
            try:
                os.mkdir(new)
            except OSError as e:
                print("Got error :%s"%e)
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
        if sys.platform == 'win32':
            # Skip this on windows as we're not enabling it due to
            # it requiring user permissions which aren't always present
            # and we don't have a good way to detect those permissions yet.
            return
        link = self.canonicalize(link)
        try:
            os.symlink(target, link)
        except AttributeError:
            pass                # Windows has no symlink

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
        drive, rest = os.path.splitdrive(path)
        if drive:
            path = drive.upper() + rest

        #
        self._dirlist.append(path)

        global _Cleanup
        if self not in _Cleanup:
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
            file = os.path.join(*tuple(file))
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
        return os.path.join(self.workdir, *tuple(args))

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
                try:
                    st = os.stat(fname)
                except OSError:
                    pass
                else:
                    os.chmod(fname, stat.S_IMODE(
                        st[stat.ST_MODE] | stat.S_IREAD))
        else:
            def do_chmod(fname):
                try:
                    st = os.stat(fname)
                except OSError:
                    pass
                else:
                    os.chmod(fname, stat.S_IMODE(
                        st[stat.ST_MODE] & ~stat.S_IREAD))

        if os.path.isfile(top):
            # If it's a file, that's easy, just chmod it.
            do_chmod(top)
        elif read:
            # It's a directory and we're trying to turn on read
            # permission, so it's also pretty easy, just chmod the
            # directory and then chmod every entry on our walk down the
            # tree.
            do_chmod(top)
            for dirpath, dirnames, filenames in os.walk(top):
                for name in dirnames + filenames:
                    do_chmod(os.path.join(dirpath, name))
        else:
            # It's a directory and we're trying to turn off read
            # permission, which means we have to chmod the directories
            # in the tree bottom-up, lest disabling read permission from
            # the top down get in the way of being able to get at lower
            # parts of the tree.
            for dirpath, dirnames, filenames in os.walk(top, topdown=0):
                for name in dirnames + filenames:
                    do_chmod(os.path.join(dirpath, name))
            do_chmod(top)

    def writable(self, top, write=1):
        """Make the specified directory tree writable (write == 1)
        or not (write == None).
        """

        if sys.platform == 'win32':

            if write:
                def do_chmod(fname):
                    try:
                        os.chmod(fname, stat.S_IWRITE)
                    except OSError:
                        pass
            else:
                def do_chmod(fname):
                    try:
                        os.chmod(fname, stat.S_IREAD)
                    except OSError:
                        pass

        else:

            if write:
                def do_chmod(fname):
                    try:
                        st = os.stat(fname)
                    except OSError:
                        pass
                    else:
                        os.chmod(fname, stat.S_IMODE(st[stat.ST_MODE] | 0o200))
            else:
                def do_chmod(fname):
                    try:
                        st = os.stat(fname)
                    except OSError:
                        pass
                    else:
                        os.chmod(fname, stat.S_IMODE(
                            st[stat.ST_MODE] & ~0o200))

        if os.path.isfile(top):
            do_chmod(top)
        else:
            do_chmod(top)
            for dirpath, dirnames, filenames in os.walk(top, topdown=0):
                for name in dirnames + filenames:
                    do_chmod(os.path.join(dirpath, name))

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
                try:
                    st = os.stat(fname)
                except OSError:
                    pass
                else:
                    os.chmod(fname, stat.S_IMODE(
                        st[stat.ST_MODE] | stat.S_IEXEC))
        else:
            def do_chmod(fname):
                try:
                    st = os.stat(fname)
                except OSError:
                    pass
                else:
                    os.chmod(fname, stat.S_IMODE(
                        st[stat.ST_MODE] & ~stat.S_IEXEC))

        if os.path.isfile(top):
            # If it's a file, that's easy, just chmod it.
            do_chmod(top)
        elif execute:
            # It's a directory and we're trying to turn on execute
            # permission, so it's also pretty easy, just chmod the
            # directory and then chmod every entry on our walk down the
            # tree.
            do_chmod(top)
            for dirpath, dirnames, filenames in os.walk(top):
                for name in dirnames + filenames:
                    do_chmod(os.path.join(dirpath, name))
        else:
            # It's a directory and we're trying to turn off execute
            # permission, which means we have to chmod the directories
            # in the tree bottom-up, lest disabling execute permission from
            # the top down get in the way of being able to get at lower
            # parts of the tree.
            for dirpath, dirnames, filenames in os.walk(top, topdown=0):
                for name in dirnames + filenames:
                    do_chmod(os.path.join(dirpath, name))
            do_chmod(top)

    def write(self, file, content, mode='wb'):
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
            raise ValueError("mode must begin with 'w'")
        with open(file, mode) as f:
            try:
                f.write(content)
            except TypeError as e:
                # python 3 default strings are not bytes, but unicode
                f.write(bytes(content, 'utf-8'))

# Local Variables:
# tab-width:4
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=4 shiftwidth=4:
