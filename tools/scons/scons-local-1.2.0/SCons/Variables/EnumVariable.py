"""engine.SCons.Variables.EnumVariable

This file defines the option type for SCons allowing only specified
input-values.

Usage example:

  opts = Variables()
  opts.Add(EnumVariable('debug', 'debug output and symbols', 'no',
                      allowed_values=('yes', 'no', 'full'),
                      map={}, ignorecase=2))
  ...
  if env['debug'] == 'full':
    ...
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

__revision__ = "src/engine/SCons/Variables/EnumVariable.py 3842 2008/12/20 22:59:52 scons"

__all__ = ['EnumVariable',]

import string

import SCons.Errors

def _validator(key, val, env, vals):
    if not val in vals:
        raise SCons.Errors.UserError(
            'Invalid value for option %s: %s' % (key, val))


def EnumVariable(key, help, default, allowed_values, map={}, ignorecase=0):
    """
    The input parameters describe a option with only certain values
    allowed. They are returned with an appropriate converter and
    validator appended. The result is usable for input to
    Variables.Add().

    'key' and 'default' are the values to be passed on to Variables.Add().

    'help' will be appended by the allowed values automatically

    'allowed_values' is a list of strings, which are allowed as values
    for this option.

    The 'map'-dictionary may be used for converting the input value
    into canonical values (eg. for aliases).

    'ignorecase' defines the behaviour of the validator:

    If ignorecase == 0, the validator/converter are case-sensitive.
    If ignorecase == 1, the validator/converter are case-insensitive.
    If ignorecase == 2, the validator/converter is case-insensitive and
                        the converted value will always be lower-case.

    The 'validator' tests whether the value is in the list of allowed
    values. The 'converter' converts input values according to the
    given 'map'-dictionary (unmapped input values are returned
    unchanged). 
    """
    help = '%s (%s)' % (help, string.join(allowed_values, '|'))
    # define validator
    if ignorecase >= 1:
        validator = lambda key, val, env, vals=allowed_values: \
                    _validator(key, string.lower(val), env, vals)
    else:
        validator = lambda key, val, env, vals=allowed_values: \
                    _validator(key, val, env, vals)
    # define converter
    if ignorecase == 2:
        converter = lambda val, map=map: \
                    string.lower(map.get(string.lower(val), val))
    elif ignorecase == 1:
        converter = lambda val, map=map: \
                    map.get(string.lower(val), val)
    else:
        converter = lambda val, map=map: \
                    map.get(val, val)
    return (key, help, default, validator, converter)
