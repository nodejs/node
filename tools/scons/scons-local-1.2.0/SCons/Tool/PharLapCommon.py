"""SCons.Tool.PharLapCommon

This module contains common code used by all Tools for the
Phar Lap ETS tool chain.  Right now, this is linkloc and
386asm.

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

__revision__ = "src/engine/SCons/Tool/PharLapCommon.py 3842 2008/12/20 22:59:52 scons"

import os
import os.path
import SCons.Errors
import SCons.Util
import re
import string

def getPharLapPath():
    """Reads the registry to find the installed path of the Phar Lap ETS
    development kit.

    Raises UserError if no installed version of Phar Lap can
    be found."""

    if not SCons.Util.can_read_reg:
        raise SCons.Errors.InternalError, "No Windows registry module was found"
    try:
        k=SCons.Util.RegOpenKeyEx(SCons.Util.HKEY_LOCAL_MACHINE,
                                  'SOFTWARE\\Pharlap\\ETS')
        val, type = SCons.Util.RegQueryValueEx(k, 'BaseDir')

        # The following is a hack...there is (not surprisingly)
        # an odd issue in the Phar Lap plug in that inserts
        # a bunch of junk data after the phar lap path in the
        # registry.  We must trim it.
        idx=val.find('\0')
        if idx >= 0:
            val = val[:idx]
                    
        return os.path.normpath(val)
    except SCons.Util.RegError:
        raise SCons.Errors.UserError, "Cannot find Phar Lap ETS path in the registry.  Is it installed properly?"

REGEX_ETS_VER = re.compile(r'#define\s+ETS_VER\s+([0-9]+)')

def getPharLapVersion():
    """Returns the version of the installed ETS Tool Suite as a
    decimal number.  This version comes from the ETS_VER #define in
    the embkern.h header.  For example, '#define ETS_VER 1010' (which
    is what Phar Lap 10.1 defines) would cause this method to return
    1010. Phar Lap 9.1 does not have such a #define, but this method
    will return 910 as a default.

    Raises UserError if no installed version of Phar Lap can
    be found."""

    include_path = os.path.join(getPharLapPath(), os.path.normpath("include/embkern.h"))
    if not os.path.exists(include_path):
        raise SCons.Errors.UserError, "Cannot find embkern.h in ETS include directory.\nIs Phar Lap ETS installed properly?"
    mo = REGEX_ETS_VER.search(open(include_path, 'r').read())
    if mo:
        return int(mo.group(1))
    # Default return for Phar Lap 9.1
    return 910

def addPathIfNotExists(env_dict, key, path, sep=os.pathsep):
    """This function will take 'key' out of the dictionary
    'env_dict', then add the path 'path' to that key if it is not
    already there.  This treats the value of env_dict[key] as if it
    has a similar format to the PATH variable...a list of paths
    separated by tokens.  The 'path' will get added to the list if it
    is not already there."""
    try:
        is_list = 1
        paths = env_dict[key]
        if not SCons.Util.is_List(env_dict[key]):
            paths = string.split(paths, sep)
            is_list = 0
        if not os.path.normcase(path) in map(os.path.normcase, paths):
            paths = [ path ] + paths
        if is_list:
            env_dict[key] = paths
        else:
            env_dict[key] = string.join(paths, sep)
    except KeyError:
        env_dict[key] = path

def addPharLapPaths(env):
    """This function adds the path to the Phar Lap binaries, includes,
    and libraries, if they are not already there."""
    ph_path = getPharLapPath()

    try:
        env_dict = env['ENV']
    except KeyError:
        env_dict = {}
        env['ENV'] = env_dict
    addPathIfNotExists(env_dict, 'PATH',
                       os.path.join(ph_path, 'bin'))
    addPathIfNotExists(env_dict, 'INCLUDE',
                       os.path.join(ph_path, 'include'))
    addPathIfNotExists(env_dict, 'LIB',
                       os.path.join(ph_path, 'lib'))
    addPathIfNotExists(env_dict, 'LIB',
                       os.path.join(ph_path, os.path.normpath('lib/vclib')))
    
    env['PHARLAP_PATH'] = getPharLapPath()
    env['PHARLAP_VERSION'] = str(getPharLapVersion())
    
