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

__revision__ = "src/engine/SCons/compat/_scons_itertools.py 3842 2008/12/20 22:59:52 scons"

__doc__ = """
Implementations of itertools functions for Python versions that don't
have iterators.

These implement the functions by creating the entire list, not returning
it element-by-element as the real itertools functions do.  This means
that early Python versions won't get the performance benefit of using
the itertools, but we can still use them so the later Python versions
do get the advantages of using iterators.

Because we return the entire list, we intentionally do not implement the
itertools functions that "return" infinitely-long lists: the count(),
cycle() and repeat() functions.  Other functions below have remained
unimplemented simply because they aren't being used (yet) and it wasn't
obvious how to do it.  Or, conversely, we only implemented those functions
that *were* easy to implement (mostly because the Python documentation
contained examples of equivalent code).

Note that these do not have independent unit tests, so it's possible
that there are bugs.
"""

def chain(*iterables):
    result = []
    for x in iterables:
        result.extend(list(x))
    return result

def count(n=0):
    # returns infinite length, should not be supported
    raise NotImplementedError

def cycle(iterable):
    # returns infinite length, should not be supported
    raise NotImplementedError

def dropwhile(predicate, iterable):
    result = []
    for x in iterable:
        if not predicate(x):
            result.append(x)
            break
    result.extend(iterable)
    return result

def groupby(iterable, *args):
    raise NotImplementedError

def ifilter(predicate, iterable):
    result = []
    if predicate is None:
        predicate = bool
    for x in iterable:
        if predicate(x):
            result.append(x)
    return result

def ifilterfalse(predicate, iterable):
    result = []
    if predicate is None:
        predicate = bool
    for x in iterable:
        if not predicate(x):
            result.append(x)
    return result

def imap(function, *iterables):
    return apply(map, (function,) + tuple(iterables))

def islice(*args, **kw):
    raise NotImplementedError

def izip(*iterables):
    return apply(zip, iterables)

def repeat(*args, **kw):
    # returns infinite length, should not be supported
    raise NotImplementedError

def starmap(*args, **kw):
    raise NotImplementedError

def takewhile(predicate, iterable):
    result = []
    for x in iterable:
        if predicate(x):
            result.append(x)
        else:
            break
    return result

def tee(*args, **kw):
    raise NotImplementedError
