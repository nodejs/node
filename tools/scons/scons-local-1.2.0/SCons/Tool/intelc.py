"""SCons.Tool.icl

Tool-specific initialization for the Intel C/C++ compiler.
Supports Linux and Windows compilers, v7 and up.

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

__revision__ = "src/engine/SCons/Tool/intelc.py 3842 2008/12/20 22:59:52 scons"

import math, sys, os.path, glob, string, re

is_windows = sys.platform == 'win32'
is_win64 = is_windows and (os.environ['PROCESSOR_ARCHITECTURE'] == 'AMD64' or 
                           (os.environ.has_key('PROCESSOR_ARCHITEW6432') and
                            os.environ['PROCESSOR_ARCHITEW6432'] == 'AMD64'))
is_linux = sys.platform == 'linux2'
is_mac     = sys.platform == 'darwin'

if is_windows:
    import SCons.Tool.msvc
elif is_linux:
    import SCons.Tool.gcc
elif is_mac:
    import SCons.Tool.gcc
import SCons.Util
import SCons.Warnings

# Exceptions for this tool
class IntelCError(SCons.Errors.InternalError):
    pass
class MissingRegistryError(IntelCError): # missing registry entry
    pass
class MissingDirError(IntelCError):     # dir not found
    pass
class NoRegistryModuleError(IntelCError): # can't read registry at all
    pass

def uniquify(s):
    """Return a sequence containing only one copy of each unique element from input sequence s.
    Does not preserve order.
    Input sequence must be hashable (i.e. must be usable as a dictionary key)."""
    u = {}
    for x in s:
        u[x] = 1
    return u.keys()

def linux_ver_normalize(vstr):
    """Normalize a Linux compiler version number.
    Intel changed from "80" to "9.0" in 2005, so we assume if the number
    is greater than 60 it's an old-style number and otherwise new-style.
    Always returns an old-style float like 80 or 90 for compatibility with Windows.
    Shades of Y2K!"""
    # Check for version number like 9.1.026: return 91.026
    m = re.match(r'([0-9]+)\.([0-9]+)\.([0-9]+)', vstr)
    if m:
        vmaj,vmin,build = m.groups()
        return float(vmaj) * 10 + float(vmin) + float(build) / 1000.;
    else:
        f = float(vstr)
        if is_windows:
            return f
        else:
            if f < 60: return f * 10.0
            else: return f

def check_abi(abi):
    """Check for valid ABI (application binary interface) name,
    and map into canonical one"""
    if not abi:
        return None
    abi = abi.lower()
    # valid_abis maps input name to canonical name
    if is_windows:
        valid_abis = {'ia32'  : 'ia32',
                      'x86'   : 'ia32',
                      'ia64'  : 'ia64',
                      'em64t' : 'em64t',
                      'amd64' : 'em64t'}
    if is_linux:
        valid_abis = {'ia32'   : 'ia32',
                      'x86'    : 'ia32',
                      'x86_64' : 'x86_64',
                      'em64t'  : 'x86_64',
                      'amd64'  : 'x86_64'}
    if is_mac:
        valid_abis = {'ia32'   : 'ia32',
                      'x86'    : 'ia32',
                      'x86_64' : 'x86_64',
                      'em64t'  : 'x86_64'}
    try:
        abi = valid_abis[abi]
    except KeyError:
        raise SCons.Errors.UserError, \
              "Intel compiler: Invalid ABI %s, valid values are %s"% \
              (abi, valid_abis.keys())
    return abi

def vercmp(a, b):
    """Compare strings as floats,
    but Intel changed Linux naming convention at 9.0"""
    return cmp(linux_ver_normalize(b), linux_ver_normalize(a))

def get_version_from_list(v, vlist):
    """See if we can match v (string) in vlist (list of strings)
    Linux has to match in a fuzzy way."""
    if is_windows:
        # Simple case, just find it in the list
        if v in vlist: return v
        else: return None
    else:
        # Fuzzy match: normalize version number first, but still return
        # original non-normalized form.
        fuzz = 0.001
        for vi in vlist:
            if math.fabs(linux_ver_normalize(vi) - linux_ver_normalize(v)) < fuzz:
                return vi
        # Not found
        return None

def get_intel_registry_value(valuename, version=None, abi=None):
    """
    Return a value from the Intel compiler registry tree. (Windows only)
    """
    # Open the key:
    if is_win64:
        K = 'Software\\Wow6432Node\\Intel\\Compilers\\C++\\' + version + '\\'+abi.upper()
    else:
        K = 'Software\\Intel\\Compilers\\C++\\' + version + '\\'+abi.upper()
    try:
        k = SCons.Util.RegOpenKeyEx(SCons.Util.HKEY_LOCAL_MACHINE, K)
    except SCons.Util.RegError:
        raise MissingRegistryError, \
              "%s was not found in the registry, for Intel compiler version %s, abi='%s'"%(K, version,abi)

    # Get the value:
    try:
        v = SCons.Util.RegQueryValueEx(k, valuename)[0]
        return v  # or v.encode('iso-8859-1', 'replace') to remove unicode?
    except SCons.Util.RegError:
        raise MissingRegistryError, \
              "%s\\%s was not found in the registry."%(K, valuename)


def get_all_compiler_versions():
    """Returns a sorted list of strings, like "70" or "80" or "9.0"
    with most recent compiler version first.
    """
    versions=[]
    if is_windows:
        if is_win64:
            keyname = 'Software\\WoW6432Node\\Intel\\Compilers\\C++'
        else:
            keyname = 'Software\\Intel\\Compilers\\C++'
        try:
            k = SCons.Util.RegOpenKeyEx(SCons.Util.HKEY_LOCAL_MACHINE,
                                        keyname)
        except WindowsError:
            return []
        i = 0
        versions = []
        try:
            while i < 100:
                subkey = SCons.Util.RegEnumKey(k, i) # raises EnvironmentError
                # Check that this refers to an existing dir.
                # This is not 100% perfect but should catch common
                # installation issues like when the compiler was installed
                # and then the install directory deleted or moved (rather
                # than uninstalling properly), so the registry values
                # are still there.
                ok = False
                for try_abi in ('IA32', 'IA32e',  'IA64', 'EM64T'):
                    try:
                        d = get_intel_registry_value('ProductDir', subkey, try_abi)
                    except MissingRegistryError:
                        continue  # not found in reg, keep going
                    if os.path.exists(d): ok = True
                if ok:
                    versions.append(subkey)
                else:
                    try:
                        # Registry points to nonexistent dir.  Ignore this
                        # version.
                        value = get_intel_registry_value('ProductDir', subkey, 'IA32')
                    except MissingRegistryError, e:

                        # Registry key is left dangling (potentially
                        # after uninstalling).

                        print \
                            "scons: *** Ignoring the registry key for the Intel compiler version %s.\n" \
                            "scons: *** It seems that the compiler was uninstalled and that the registry\n" \
                            "scons: *** was not cleaned up properly.\n" % subkey
                    else:
                        print "scons: *** Ignoring "+str(value)

                i = i + 1
        except EnvironmentError:
            # no more subkeys
            pass
    elif is_linux:
        for d in glob.glob('/opt/intel_cc_*'):
            # Typical dir here is /opt/intel_cc_80.
            m = re.search(r'cc_(.*)$', d)
            if m:
                versions.append(m.group(1))
        for d in glob.glob('/opt/intel/cc*/*'):
            # Typical dir here is /opt/intel/cc/9.0 for IA32,
            # /opt/intel/cce/9.0 for EMT64 (AMD64)
            m = re.search(r'([0-9.]+)$', d)
            if m:
                versions.append(m.group(1))
    elif is_mac:
        for d in glob.glob('/opt/intel/cc*/*'):
            # Typical dir here is /opt/intel/cc/9.0 for IA32,
            # /opt/intel/cce/9.0 for EMT64 (AMD64)
            m = re.search(r'([0-9.]+)$', d)
            if m:
                versions.append(m.group(1))
    versions = uniquify(versions)       # remove dups
    versions.sort(vercmp)
    return versions

def get_intel_compiler_top(version, abi):
    """
    Return the main path to the top-level dir of the Intel compiler,
    using the given version.
    The compiler will be in <top>/bin/icl.exe (icc on linux),
    the include dir is <top>/include, etc.
    """

    if is_windows:
        if not SCons.Util.can_read_reg:
            raise NoRegistryModuleError, "No Windows registry module was found"
        top = get_intel_registry_value('ProductDir', version, abi)
        if not os.path.exists(os.path.join(top, "Bin", "icl.exe")):
            raise MissingDirError, \
                  "Can't find Intel compiler in %s"%(top)
    elif is_mac or is_linux:
        # first dir is new (>=9.0) style, second is old (8.0) style.
        dirs=('/opt/intel/cc/%s', '/opt/intel_cc_%s')
        if abi == 'x86_64':
            dirs=('/opt/intel/cce/%s',)  # 'e' stands for 'em64t', aka x86_64 aka amd64
        top=None
        for d in dirs:
            if os.path.exists(os.path.join(d%version, "bin", "icc")):
                top = d%version
                break
        if not top:
            raise MissingDirError, \
                  "Can't find version %s Intel compiler in %s (abi='%s')"%(version,top, abi)
    return top


def generate(env, version=None, abi=None, topdir=None, verbose=0):
    """Add Builders and construction variables for Intel C/C++ compiler
    to an Environment.
    args:
      version: (string) compiler version to use, like "80"
      abi:     (string) 'win32' or whatever Itanium version wants
      topdir:  (string) compiler top dir, like
                         "c:\Program Files\Intel\Compiler70"
                        If topdir is used, version and abi are ignored.
      verbose: (int)    if >0, prints compiler version used.
    """
    if not (is_mac or is_linux or is_windows):
        # can't handle this platform
        return

    if is_windows:
        SCons.Tool.msvc.generate(env)
    elif is_linux:
        SCons.Tool.gcc.generate(env)
    elif is_mac:
        SCons.Tool.gcc.generate(env)

    # if version is unspecified, use latest
    vlist = get_all_compiler_versions()
    if not version:
        if vlist:
            version = vlist[0]
    else:
        # User may have specified '90' but we need to get actual dirname '9.0'.
        # get_version_from_list does that mapping.
        v = get_version_from_list(version, vlist)
        if not v:
            raise SCons.Errors.UserError, \
                  "Invalid Intel compiler version %s: "%version + \
                  "installed versions are %s"%(', '.join(vlist))
        version = v

    # if abi is unspecified, use ia32
    # alternatives are ia64 for Itanium, or amd64 or em64t or x86_64 (all synonyms here)
    abi = check_abi(abi)
    if abi is None:
        if is_mac or is_linux:
            # Check if we are on 64-bit linux, default to 64 then.
            uname_m = os.uname()[4]
            if uname_m == 'x86_64':
                abi = 'x86_64'
            else:
                abi = 'ia32'
        else:
            if is_win64:
                abi = 'em64t'
            else:
                abi = 'ia32'

    if version and not topdir:
        try:
            topdir = get_intel_compiler_top(version, abi)
        except (SCons.Util.RegError, IntelCError):
            topdir = None

    if not topdir:
        # Normally this is an error, but it might not be if the compiler is
        # on $PATH and the user is importing their env.
        class ICLTopDirWarning(SCons.Warnings.Warning):
            pass
        if (is_mac or is_linux) and not env.Detect('icc') or \
           is_windows and not env.Detect('icl'):

            SCons.Warnings.enableWarningClass(ICLTopDirWarning)
            SCons.Warnings.warn(ICLTopDirWarning,
                                "Failed to find Intel compiler for version='%s', abi='%s'"%
                                (str(version), str(abi)))
        else:
            # should be cleaned up to say what this other version is
            # since in this case we have some other Intel compiler installed
            SCons.Warnings.enableWarningClass(ICLTopDirWarning)
            SCons.Warnings.warn(ICLTopDirWarning,
                                "Can't find Intel compiler top dir for version='%s', abi='%s'"%
                                    (str(version), str(abi)))

    if topdir:
        if verbose:
            print "Intel C compiler: using version %s (%g), abi %s, in '%s'"%\
                  (repr(version), linux_ver_normalize(version),abi,topdir)
            if is_linux:
                # Show the actual compiler version by running the compiler.
                os.system('%s/bin/icc --version'%topdir)
            if is_mac:
                # Show the actual compiler version by running the compiler.
                os.system('%s/bin/icc --version'%topdir)

        env['INTEL_C_COMPILER_TOP'] = topdir
        if is_linux:
            paths={'INCLUDE'         : 'include',
                   'LIB'             : 'lib',
                   'PATH'            : 'bin',
                   'LD_LIBRARY_PATH' : 'lib'}
            for p in paths.keys():
                env.PrependENVPath(p, os.path.join(topdir, paths[p]))
        if is_mac:
            paths={'INCLUDE'         : 'include',
                   'LIB'             : 'lib',
                   'PATH'            : 'bin',
                   'LD_LIBRARY_PATH' : 'lib'}
            for p in paths.keys():
                env.PrependENVPath(p, os.path.join(topdir, paths[p]))
        if is_windows:
            #       env key    reg valname   default subdir of top
            paths=(('INCLUDE', 'IncludeDir', 'Include'),
                   ('LIB'    , 'LibDir',     'Lib'),
                   ('PATH'   , 'BinDir',     'Bin'))
            # We are supposed to ignore version if topdir is set, so set
            # it to the emptry string if it's not already set.
            if version is None:
                version = ''
            # Each path has a registry entry, use that or default to subdir
            for p in paths:
                try:
                    path=get_intel_registry_value(p[1], version, abi)
                    # These paths may have $(ICInstallDir)
                    # which needs to be substituted with the topdir.
                    path=path.replace('$(ICInstallDir)', topdir + os.sep)
                except IntelCError:
                    # Couldn't get it from registry: use default subdir of topdir
                    env.PrependENVPath(p[0], os.path.join(topdir, p[2]))
                else:
                    env.PrependENVPath(p[0], string.split(path, os.pathsep))
                    # print "ICL %s: %s, final=%s"%(p[0], path, str(env['ENV'][p[0]]))

    if is_windows:
        env['CC']        = 'icl'
        env['CXX']       = 'icl'
        env['LINK']      = 'xilink'
    else:
        env['CC']        = 'icc'
        env['CXX']       = 'icpc'
        # Don't reset LINK here;
        # use smart_link which should already be here from link.py.
        #env['LINK']      = '$CC'
        env['AR']        = 'xiar'
        env['LD']        = 'xild' # not used by default

    # This is not the exact (detailed) compiler version,
    # just the major version as determined above or specified
    # by the user.  It is a float like 80 or 90, in normalized form for Linux
    # (i.e. even for Linux 9.0 compiler, still returns 90 rather than 9.0)
    if version:
        env['INTEL_C_COMPILER_VERSION']=linux_ver_normalize(version)

    if is_windows:
        # Look for license file dir
        # in system environment, registry, and default location.
        envlicdir = os.environ.get("INTEL_LICENSE_FILE", '')
        K = ('SOFTWARE\Intel\Licenses')
        try:
            k = SCons.Util.RegOpenKeyEx(SCons.Util.HKEY_LOCAL_MACHINE, K)
            reglicdir = SCons.Util.RegQueryValueEx(k, "w_cpp")[0]
        except (AttributeError, SCons.Util.RegError):
            reglicdir = ""
        defaultlicdir = r'C:\Program Files\Common Files\Intel\Licenses'

        licdir = None
        for ld in [envlicdir, reglicdir]:
            # If the string contains an '@', then assume it's a network
            # license (port@system) and good by definition.
            if ld and (string.find(ld, '@') != -1 or os.path.exists(ld)):
                licdir = ld
                break
        if not licdir:
            licdir = defaultlicdir
            if not os.path.exists(licdir):
                class ICLLicenseDirWarning(SCons.Warnings.Warning):
                    pass
                SCons.Warnings.enableWarningClass(ICLLicenseDirWarning)
                SCons.Warnings.warn(ICLLicenseDirWarning,
                                    "Intel license dir was not found."
                                    "  Tried using the INTEL_LICENSE_FILE environment variable (%s), the registry (%s) and the default path (%s)."
                                    "  Using the default path as a last resort."
                                        % (envlicdir, reglicdir, defaultlicdir))
        env['ENV']['INTEL_LICENSE_FILE'] = licdir

def exists(env):
    if not (is_mac or is_linux or is_windows):
        # can't handle this platform
        return 0

    try:
        versions = get_all_compiler_versions()
    except (SCons.Util.RegError, IntelCError):
        versions = None
    detected = versions is not None and len(versions) > 0
    if not detected:
        # try env.Detect, maybe that will work
        if is_windows:
            return env.Detect('icl')
        elif is_linux:
            return env.Detect('icc')
        elif is_mac:
            return env.Detect('icc')
    return detected

# end of file
