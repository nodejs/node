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

# Portions of the following are derived from the compat.py file in
# Twisted, under the following copyright:
#
# Copyright (c) 2001-2004 Twisted Matrix Laboratories

__doc__ = """
Compatibility idioms for __builtin__ names

This module adds names to the __builtin__ module for things that we want
to use in SCons but which don't show up until later Python versions than
the earliest ones we support.

This module checks for the following __builtin__ names:

        all()
        any()
        bool()
        dict()
        True
        False
        zip()

Implementations of functions are *NOT* guaranteed to be fully compliant
with these functions in later versions of Python.  We are only concerned
with adding functionality that we actually use in SCons, so be wary
if you lift this code for other uses.  (That said, making these more
nearly the same as later, official versions is still a desirable goal,
we just don't need to be obsessive about it.)

If you're looking at this with pydoc and various names don't show up in
the FUNCTIONS or DATA output, that means those names are already built in
to this version of Python and we don't need to add them from this module.
"""

__revision__ = "src/engine/SCons/compat/builtins.py 3842 2008/12/20 22:59:52 scons"

import __builtin__

try:
    all
except NameError:
    # Pre-2.5 Python has no all() function.
    def all(iterable):
        """
        Returns True if all elements of the iterable are true.
        """
        for element in iterable:
            if not element:
                return False
        return True
    __builtin__.all = all
    all = all

try:
    any
except NameError:
    # Pre-2.5 Python has no any() function.
    def any(iterable):
        """
        Returns True if any element of the iterable is true.
        """
        for element in iterable:
            if element:
                return True
        return False
    __builtin__.any = any
    any = any

try:
    bool
except NameError:
    # Pre-2.2 Python has no bool() function.
    def bool(value):
        """Demote a value to 0 or 1, depending on its truth value.

        This is not to be confused with types.BooleanType, which is
        way too hard to duplicate in early Python versions to be
        worth the trouble.
        """
        return not not value
    __builtin__.bool = bool
    bool = bool

try:
    dict
except NameError:
    # Pre-2.2 Python has no dict() keyword.
    def dict(seq=[], **kwargs):
        """
        New dictionary initialization.
        """
        d = {}
        for k, v in seq:
            d[k] = v
        d.update(kwargs)
        return d
    __builtin__.dict = dict

try:
    False
except NameError:
    # Pre-2.2 Python has no False keyword.
    __builtin__.False = not 1
    # Assign to False in this module namespace so it shows up in pydoc output.
    False = False

try:
    True
except NameError:
    # Pre-2.2 Python has no True keyword.
    __builtin__.True = not 0
    # Assign to True in this module namespace so it shows up in pydoc output.
    True = True

try:
    file
except NameError:
    # Pre-2.2 Python has no file() function.
    __builtin__.file = open

#
try:
    zip
except NameError:
    # Pre-2.2 Python has no zip() function.
    def zip(*lists):
        """
        Emulates the behavior we need from the built-in zip() function
        added in Python 2.2.

        Returns a list of tuples, where each tuple contains the i-th
        element rom each of the argument sequences.  The returned
        list is truncated in length to the length of the shortest
        argument sequence.
        """
        result = []
        for i in xrange(min(map(len, lists))):
            result.append(tuple(map(lambda l, i=i: l[i], lists)))
        return result
    __builtin__.zip = zip



#if sys.version_info[:3] in ((2, 2, 0), (2, 2, 1)):
#    def lstrip(s, c=string.whitespace):
#        while s and s[0] in c:
#            s = s[1:]
#        return s
#    def rstrip(s, c=string.whitespace):
#        while s and s[-1] in c:
#            s = s[:-1]
#        return s
#    def strip(s, c=string.whitespace, l=lstrip, r=rstrip):
#        return l(r(s, c), c)
#
#    object.__setattr__(str, 'lstrip', lstrip)
#    object.__setattr__(str, 'rstrip', rstrip)
#    object.__setattr__(str, 'strip', strip)
