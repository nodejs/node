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

__revision__ = "src/engine/SCons/Sig.py 3842 2008/12/20 22:59:52 scons"

__doc__ = """Place-holder for the old SCons.Sig module hierarchy

This is no longer used, but code out there (such as the NSIS module on
the SCons wiki) may try to import SCons.Sig.  If so, we generate a warning
that points them to the line that caused the import, and don't die.

If someone actually tried to use the sub-modules or functions within
the package (for example, SCons.Sig.MD5.signature()), then they'll still
get an AttributeError, but at least they'll know where to start looking.
"""

import SCons.Util
import SCons.Warnings

msg = 'The SCons.Sig module no longer exists.\n' \
      '    Remove the following "import SCons.Sig" line to eliminate this warning:'

SCons.Warnings.warn(SCons.Warnings.DeprecatedWarning, msg)

default_calc = None
default_module = None

class MD5Null(SCons.Util.Null):
    def __repr__(self):
        return "MD5Null()"

class TimeStampNull(SCons.Util.Null):
    def __repr__(self):
        return "TimeStampNull()"

MD5 = MD5Null()
TimeStamp = TimeStampNull()
