"""engine.SCons.Variables.ListVariable

This file defines the option type for SCons implementing 'lists'.

A 'list' option may either be 'all', 'none' or a list of names
separated by comma. After the option has been processed, the option
value holds either the named list elements, all list elemens or no
list elements at all.

Usage example:

  list_of_libs = Split('x11 gl qt ical')

  opts = Variables()
  opts.Add(ListVariable('shared',
                      'libraries to build as shared libraries',
                      'all',
                      elems = list_of_libs))
  ...
  for lib in list_of_libs:
     if lib in env['shared']:
         env.SharedObject(...)
     else:
         env.Object(...)
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

__revision__ = "src/engine/SCons/Variables/ListVariable.py 3842 2008/12/20 22:59:52 scons"

# Know Bug: This should behave like a Set-Type, but does not really,
# since elements can occur twice.

__all__ = ['ListVariable',]

import string
import UserList

import SCons.Util


class _ListVariable(UserList.UserList):
    def __init__(self, initlist=[], allowedElems=[]):
        UserList.UserList.__init__(self, filter(None, initlist))
        self.allowedElems = allowedElems[:]
        self.allowedElems.sort()

    def __cmp__(self, other):
        raise NotImplementedError
    def __eq__(self, other):
        raise NotImplementedError
    def __ge__(self, other):
        raise NotImplementedError
    def __gt__(self, other):
        raise NotImplementedError
    def __le__(self, other):
        raise NotImplementedError
    def __lt__(self, other):
        raise NotImplementedError
    def __str__(self):
        if len(self) == 0:
            return 'none'
        self.data.sort()
        if self.data == self.allowedElems:
            return 'all'
        else:
            return string.join(self, ',')
    def prepare_to_store(self):
        return self.__str__()

def _converter(val, allowedElems, mapdict):
    """
    """
    if val == 'none':
        val = []
    elif val == 'all':
        val = allowedElems
    else:
        val = filter(None, string.split(val, ','))
        val = map(lambda v, m=mapdict: m.get(v, v), val)
        notAllowed = filter(lambda v, aE=allowedElems: not v in aE, val)
        if notAllowed:
            raise ValueError("Invalid value(s) for option: %s" %
                             string.join(notAllowed, ','))
    return _ListVariable(val, allowedElems)


## def _validator(key, val, env):
##     """
##     """
##     # todo: write validater for pgk list
##     return 1


def ListVariable(key, help, default, names, map={}):
    """
    The input parameters describe a 'package list' option, thus they
    are returned with the correct converter and validater appended. The
    result is usable for input to opts.Add() .

    A 'package list' option may either be 'all', 'none' or a list of
    package names (separated by space).
    """
    names_str = 'allowed names: %s' % string.join(names, ' ')
    if SCons.Util.is_List(default):
        default = string.join(default, ',')
    help = string.join(
        (help, '(all|none|comma-separated list of names)', names_str),
        '\n    ')
    return (key, help, default,
            None, #_validator,
            lambda val, elems=names, m=map: _converter(val, elems, m))
