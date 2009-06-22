"""SCons.Tool.mwcc

Tool-specific initialization for the Metrowerks CodeWarrior compiler.

There normally shouldn't be any need to import this module directly.
It will usually be imported through the generic SCons.Tool.Tool()
selection method.
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

__revision__ = "src/engine/SCons/Tool/mwcc.py 3842 2008/12/20 22:59:52 scons"

import os
import os.path
import string

import SCons.Util

def set_vars(env):
    """Set MWCW_VERSION, MWCW_VERSIONS, and some codewarrior environment vars

    MWCW_VERSIONS is set to a list of objects representing installed versions

    MWCW_VERSION  is set to the version object that will be used for building.
                  MWCW_VERSION can be set to a string during Environment
                  construction to influence which version is chosen, otherwise
                  the latest one from MWCW_VERSIONS is used.

    Returns true if at least one version is found, false otherwise
    """
    desired = env.get('MWCW_VERSION', '')

    # return right away if the variables are already set
    if isinstance(desired, MWVersion):
        return 1
    elif desired is None:
        return 0

    versions = find_versions()
    version = None

    if desired:
        for v in versions:
            if str(v) == desired:
                version = v
    elif versions:
        version = versions[-1]

    env['MWCW_VERSIONS'] = versions
    env['MWCW_VERSION'] = version

    if version is None:
      return 0

    env.PrependENVPath('PATH', version.clpath)
    env.PrependENVPath('PATH', version.dllpath)
    ENV = env['ENV']
    ENV['CWFolder'] = version.path
    ENV['LM_LICENSE_FILE'] = version.license
    plus = lambda x: '+%s' % x
    ENV['MWCIncludes'] = string.join(map(plus, version.includes), os.pathsep)
    ENV['MWLibraries'] = string.join(map(plus, version.libs), os.pathsep)
    return 1


def find_versions():
    """Return a list of MWVersion objects representing installed versions"""
    versions = []

    ### This function finds CodeWarrior by reading from the registry on
    ### Windows. Some other method needs to be implemented for other
    ### platforms, maybe something that calls env.WhereIs('mwcc')

    if SCons.Util.can_read_reg:
        try:
            HLM = SCons.Util.HKEY_LOCAL_MACHINE
            product = 'SOFTWARE\\Metrowerks\\CodeWarrior\\Product Versions'
            product_key = SCons.Util.RegOpenKeyEx(HLM, product)

            i = 0
            while 1:
                name = product + '\\' + SCons.Util.RegEnumKey(product_key, i)
                name_key = SCons.Util.RegOpenKeyEx(HLM, name)

                try:
                    version = SCons.Util.RegQueryValueEx(name_key, 'VERSION')
                    path = SCons.Util.RegQueryValueEx(name_key, 'PATH')
                    mwv = MWVersion(version[0], path[0], 'Win32-X86')
                    versions.append(mwv)
                except SCons.Util.RegError:
                    pass

                i = i + 1

        except SCons.Util.RegError:
            pass

    return versions


class MWVersion:
    def __init__(self, version, path, platform):
        self.version = version
        self.path = path
        self.platform = platform
        self.clpath = os.path.join(path, 'Other Metrowerks Tools',
                                   'Command Line Tools')
        self.dllpath = os.path.join(path, 'Bin')

        # The Metrowerks tools don't store any configuration data so they
        # are totally dumb when it comes to locating standard headers,
        # libraries, and other files, expecting all the information
        # to be handed to them in environment variables. The members set
        # below control what information scons injects into the environment

        ### The paths below give a normal build environment in CodeWarrior for
        ### Windows, other versions of CodeWarrior might need different paths.

        msl = os.path.join(path, 'MSL')
        support = os.path.join(path, '%s Support' % platform)

        self.license = os.path.join(path, 'license.dat')
        self.includes = [msl, support]
        self.libs = [msl, support]

    def __str__(self):
        return self.version


CSuffixes = ['.c', '.C']
CXXSuffixes = ['.cc', '.cpp', '.cxx', '.c++', '.C++']


def generate(env):
    """Add Builders and construction variables for the mwcc to an Environment."""
    import SCons.Defaults
    import SCons.Tool

    set_vars(env)

    static_obj, shared_obj = SCons.Tool.createObjBuilders(env)

    for suffix in CSuffixes:
        static_obj.add_action(suffix, SCons.Defaults.CAction)
        shared_obj.add_action(suffix, SCons.Defaults.ShCAction)

    for suffix in CXXSuffixes:
        static_obj.add_action(suffix, SCons.Defaults.CXXAction)
        shared_obj.add_action(suffix, SCons.Defaults.ShCXXAction)

    env['CCCOMFLAGS'] = '$CPPFLAGS $_CPPDEFFLAGS $_CPPINCFLAGS -nolink -o $TARGET $SOURCES'

    env['CC']         = 'mwcc'
    env['CCCOM']      = '$CC $CFLAGS $CCFLAGS $CCCOMFLAGS'

    env['CXX']        = 'mwcc'
    env['CXXCOM']     = '$CXX $CXXFLAGS $CCCOMFLAGS'

    env['SHCC']       = '$CC'
    env['SHCCFLAGS']  = '$CCFLAGS'
    env['SHCFLAGS']   = '$CFLAGS'
    env['SHCCCOM']    = '$SHCC $SHCFLAGS $SHCCFLAGS $CCCOMFLAGS'

    env['SHCXX']       = '$CXX'
    env['SHCXXFLAGS']  = '$CXXFLAGS'
    env['SHCXXCOM']    = '$SHCXX $SHCXXFLAGS $CCCOMFLAGS'

    env['CFILESUFFIX'] = '.c'
    env['CXXFILESUFFIX'] = '.cpp'
    env['CPPDEFPREFIX']  = '-D'
    env['CPPDEFSUFFIX']  = ''
    env['INCPREFIX']  = '-I'
    env['INCSUFFIX']  = ''

    #env['PCH'] = ?
    #env['PCHSTOP'] = ?


def exists(env):
    return set_vars(env)
