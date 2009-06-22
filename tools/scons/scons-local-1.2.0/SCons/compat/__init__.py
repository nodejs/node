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

__doc__ = """
SCons compatibility package for old Python versions

This subpackage holds modules that provide backwards-compatible
implementations of various things that we'd like to use in SCons but which
only show up in later versions of Python than the early, old version(s)
we still support.

Other code will not generally reference things in this package through
the SCons.compat namespace.  The modules included here add things to
the __builtin__ namespace or the global module list so that the rest
of our code can use the objects and names imported here regardless of
Python version.

Simply enough, things that go in the __builtin__ name space come from
our builtins module.

The rest of the things here will be in individual compatibility modules
that are either: 1) suitably modified copies of the future modules that
we want to use; or 2) backwards compatible re-implementations of the
specific portions of a future module's API that we want to use.

GENERAL WARNINGS:  Implementations of functions in the SCons.compat
modules are *NOT* guaranteed to be fully compliant with these functions in
later versions of Python.  We are only concerned with adding functionality
that we actually use in SCons, so be wary if you lift this code for
other uses.  (That said, making these more nearly the same as later,
official versions is still a desirable goal, we just don't need to be
obsessive about it.)

We name the compatibility modules with an initial '_scons_' (for example,
_scons_subprocess.py is our compatibility module for subprocess) so
that we can still try to import the real module name and fall back to
our compatibility module if we get an ImportError.  The import_as()
function defined below loads the module as the "real" name (without the
'_scons'), after which all of the "import {module}" statements in the
rest of our code will find our pre-loaded compatibility module.
"""

__revision__ = "src/engine/SCons/compat/__init__.py 3842 2008/12/20 22:59:52 scons"

def import_as(module, name):
    """
    Imports the specified module (from our local directory) as the
    specified name.
    """
    import imp
    import os.path
    dir = os.path.split(__file__)[0]
    file, filename, suffix_mode_type = imp.find_module(module, [dir])
    imp.load_module(name, file, filename, suffix_mode_type)

import builtins

try:
    import hashlib
except ImportError:
    # Pre-2.5 Python has no hashlib module.
    try:
        import_as('_scons_hashlib', 'hashlib')
    except ImportError:
        # If we failed importing our compatibility module, it probably
        # means this version of Python has no md5 module.  Don't do
        # anything and let the higher layer discover this fact, so it
        # can fall back to using timestamp.
        pass

try:
    set
except NameError:
    # Pre-2.4 Python has no native set type
    try:
        # Python 2.2 and 2.3 can use the copy of the 2.[45] sets module
        # that we grabbed.
        import_as('_scons_sets', 'sets')
    except (ImportError, SyntaxError):
        # Python 1.5 (ImportError, no __future_ module) and 2.1
        # (SyntaxError, no generators in __future__) will blow up
        # trying to import the 2.[45] sets module, so back off to a
        # custom sets module that can be discarded easily when we
        # stop supporting those versions.
        import_as('_scons_sets15', 'sets')
    import __builtin__
    import sets
    __builtin__.set = sets.Set

import fnmatch
try:
    fnmatch.filter
except AttributeError:
    # Pre-2.2 Python has no fnmatch.filter() function.
    def filter(names, pat):
        """Return the subset of the list NAMES that match PAT"""
        import os,posixpath
        result=[]
        pat = os.path.normcase(pat)
        if not fnmatch._cache.has_key(pat):
            import re
            res = fnmatch.translate(pat)
            fnmatch._cache[pat] = re.compile(res)
        match = fnmatch._cache[pat].match
        if os.path is posixpath:
            # normcase on posix is NOP. Optimize it away from the loop.
            for name in names:
                if match(name):
                    result.append(name)
        else:
            for name in names:
                if match(os.path.normcase(name)):
                    result.append(name)
        return result
    fnmatch.filter = filter
    del filter

try:
    import itertools
except ImportError:
    # Pre-2.3 Python has no itertools module.
    import_as('_scons_itertools', 'itertools')

# If we need the compatibility version of textwrap, it  must be imported
# before optparse, which uses it.
try:
    import textwrap
except ImportError:
    # Pre-2.3 Python has no textwrap module.
    import_as('_scons_textwrap', 'textwrap')

try:
    import optparse
except ImportError:
    # Pre-2.3 Python has no optparse module.
    import_as('_scons_optparse', 'optparse')

import os
try:
    os.devnull
except AttributeError:
    # Pre-2.4 Python has no os.devnull attribute
    import sys
    _names = sys.builtin_module_names
    if 'posix' in _names:
        os.devnull = '/dev/null'
    elif 'nt' in _names:
        os.devnull = 'nul'
    os.path.devnull = os.devnull

import shlex
try:
    shlex.split
except AttributeError:
    # Pre-2.3 Python has no shlex.split() function.
    #
    # The full white-space splitting semantics of shlex.split() are
    # complicated to reproduce by hand, so just use a compatibility
    # version of the shlex module cribbed from Python 2.5 with some
    # minor modifications for older Python versions.
    del shlex
    import_as('_scons_shlex', 'shlex')


import shutil
try:
    shutil.move
except AttributeError:
    # Pre-2.3 Python has no shutil.move() function.
    #
    # Cribbed from Python 2.5.
    import os

    def move(src, dst):
        """Recursively move a file or directory to another location.

        If the destination is on our current filesystem, then simply use
        rename.  Otherwise, copy src to the dst and then remove src.
        A lot more could be done here...  A look at a mv.c shows a lot of
        the issues this implementation glosses over.

        """
        try:
            os.rename(src, dst)
        except OSError:
            if os.path.isdir(src):
                if shutil.destinsrc(src, dst):
                    raise Error, "Cannot move a directory '%s' into itself '%s'." % (src, dst)
                shutil.copytree(src, dst, symlinks=True)
                shutil.rmtree(src)
            else:
                shutil.copy2(src,dst)
                os.unlink(src)
    shutil.move = move
    del move

    def destinsrc(src, dst):
        src = os.path.abspath(src)
        return os.path.abspath(dst)[:len(src)] == src
    shutil.destinsrc = destinsrc
    del destinsrc


try:
    import subprocess
except ImportError:
    # Pre-2.4 Python has no subprocess module.
    import_as('_scons_subprocess', 'subprocess')

import sys
try:
    sys.version_info
except AttributeError:
    # Pre-1.6 Python has no sys.version_info
    import string
    version_string = string.split(sys.version)[0]
    version_ints = map(int, string.split(version_string, '.'))
    sys.version_info = tuple(version_ints + ['final', 0])

try:
    import UserString
except ImportError:
    # Pre-1.6 Python has no UserString module.
    import_as('_scons_UserString', 'UserString')
