"""engine.SCons.Variables.PackageVariable

This file defines the option type for SCons implementing 'package
activation'.

To be used whenever a 'package' may be enabled/disabled and the
package path may be specified.

Usage example:

  Examples:
      x11=no   (disables X11 support)
      x11=yes  (will search for the package installation dir)
      x11=/usr/local/X11 (will check this path for existance)

  To replace autoconf's --with-xxx=yyy 

  opts = Variables()
  opts.Add(PackageVariable('x11',
                         'use X11 installed here (yes = search some places',
                         'yes'))
  ...
  if env['x11'] == True:
      dir = ... search X11 in some standard places ...
      env['x11'] = dir 
  if env['x11']:
      ... build with x11 ...
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

__revision__ = "src/engine/SCons/Variables/PackageVariable.py 3842 2008/12/20 22:59:52 scons"

__all__ = ['PackageVariable',]

import string

import SCons.Errors

__enable_strings  = ('1', 'yes', 'true',  'on', 'enable', 'search')
__disable_strings = ('0', 'no',  'false', 'off', 'disable')

def _converter(val):
    """
    """
    lval = string.lower(val)
    if lval in __enable_strings: return True
    if lval in __disable_strings: return False
    #raise ValueError("Invalid value for boolean option: %s" % val)
    return val


def _validator(key, val, env, searchfunc):
    # NB: searchfunc is currenty undocumented and unsupported
    """
    """
    # todo: write validator, check for path
    import os
    if env[key] is True:
        if searchfunc:
            env[key] = searchfunc(key, val)
    elif env[key] and not os.path.exists(val):
        raise SCons.Errors.UserError(
            'Path does not exist for option %s: %s' % (key, val))


def PackageVariable(key, help, default, searchfunc=None):
    # NB: searchfunc is currenty undocumented and unsupported
    """
    The input parameters describe a 'package list' option, thus they
    are returned with the correct converter and validator appended. The
    result is usable for input to opts.Add() .

    A 'package list' option may either be 'all', 'none' or a list of
    package names (seperated by space).
    """
    help = string.join(
        (help, '( yes | no | /path/to/%s )' % key),
        '\n    ')
    return (key, help, default,
            lambda k, v, e, f=searchfunc: _validator(k,v,e,f),
            _converter)
