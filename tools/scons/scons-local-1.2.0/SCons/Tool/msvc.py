"""engine.SCons.Tool.msvc

Tool-specific initialization for Microsoft Visual C/C++.

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

__revision__ = "src/engine/SCons/Tool/msvc.py 3842 2008/12/20 22:59:52 scons"

import os.path
import re
import string

import SCons.Action
import SCons.Builder
import SCons.Errors
import SCons.Platform.win32
import SCons.Tool
import SCons.Tool.msvs
import SCons.Util
import SCons.Warnings
import SCons.Scanner.RC

CSuffixes = ['.c', '.C']
CXXSuffixes = ['.cc', '.cpp', '.cxx', '.c++', '.C++']

def _parse_msvc7_overrides(version,platform):
    """ Parse any overridden defaults for MSVS directory locations
    in MSVS .NET. """

    # First, we get the shell folder for this user:
    if not SCons.Util.can_read_reg:
        raise SCons.Errors.InternalError, "No Windows registry module was found"

    comps = ""
    try:
        (comps, t) = SCons.Util.RegGetValue(SCons.Util.HKEY_CURRENT_USER,
                                            r'Software\Microsoft\Windows\CurrentVersion' +\
                                            r'\Explorer\Shell Folders\Local AppData')
    except SCons.Util.RegError:
        raise SCons.Errors.InternalError, \
              "The Local AppData directory was not found in the registry."

    comps = comps + '\\Microsoft\\VisualStudio\\' + version + '\\VCComponents.dat'
    dirs = {}

    if os.path.exists(comps):
        # now we parse the directories from this file, if it exists.
        # We only look for entries after:
        # [VC\VC_OBJECTS_PLATFORM_INFO\Win32\Directories],
        # since this file could contain a number of things...
        lines = None
        try:
            import codecs
        except ImportError:
            pass
        else:
            try:
                f = codecs.open(comps, 'r', 'utf16')
                encoder = codecs.getencoder('ascii')
                lines = map(lambda l, e=encoder: e(l)[0], f.readlines())
            except (LookupError, UnicodeError):
                lines = codecs.open(comps, 'r', 'utf8').readlines()
        if lines is None:
            lines = open(comps, 'r').readlines()
        if 'x86' == platform: platform = 'Win32'

        found = 0
        for line in lines:
            line.strip()
            if line.find(r'[VC\VC_OBJECTS_PLATFORM_INFO\%s\Directories]'%platform) >= 0:
                found = 1
            elif line == '' or line[:1] == '[':
                found = 0
            elif found == 1:
                kv = line.split('=', 1)
                if len(kv) == 2:
                    (key, val) = kv
                key = key.replace(' Dirs','')
                dirs[key.upper()] = val
        f.close()
    else:
        # since the file didn't exist, we have only the defaults in
        # the registry to work with.

        if 'x86' == platform: platform = 'Win32'

        try:
            K = 'SOFTWARE\\Microsoft\\VisualStudio\\' + version
            K = K + r'\VC\VC_OBJECTS_PLATFORM_INFO\%s\Directories'%platform
            k = SCons.Util.RegOpenKeyEx(SCons.Util.HKEY_LOCAL_MACHINE,K)
            i = 0
            while 1:
                try:
                    (key,val,t) = SCons.Util.RegEnumValue(k,i)
                    key = key.replace(' Dirs','')
                    dirs[key.upper()] = val
                    i = i + 1
                except SCons.Util.RegError:
                    break
        except SCons.Util.RegError:
            # if we got here, then we didn't find the registry entries:
            raise SCons.Errors.InternalError, "Unable to find MSVC paths in the registry."
    return dirs

def _parse_msvc8_overrides(version,platform,suite):
    """ Parse any overridden defaults for MSVC directory locations
    in MSVC 2005. """

    # In VS8 the user can change the location of the settings file that
    # contains the include, lib and binary paths. Try to get the location
    # from registry
    if not SCons.Util.can_read_reg:
        raise SCons.Errors.InternalError, "No Windows registry module was found"

    # XXX This code assumes anything that isn't EXPRESS uses the default
    # registry key string.  Is this really true for all VS suites?
    if suite == 'EXPRESS':
        s = '\\VCExpress\\'
    else:
        s = '\\VisualStudio\\'

    settings_path = ""
    try:
        (settings_path, t) = SCons.Util.RegGetValue(SCons.Util.HKEY_CURRENT_USER,
                                                    r'Software\Microsoft' + s + version +\
                                                    r'\Profile\AutoSaveFile')
        settings_path = settings_path.upper()
    except SCons.Util.RegError:
        raise SCons.Errors.InternalError, \
              "The VS8 settings file location was not found in the registry."

    # Look for potential environment variables in the settings path
    if settings_path.find('%VSSPV_VISUALSTUDIO_DIR%') >= 0:
        # First replace a special variable named %vsspv_visualstudio_dir%
        # that is not found in the OSs environment variables...
        try:
            (value, t) = SCons.Util.RegGetValue(SCons.Util.HKEY_CURRENT_USER,
                                                r'Software\Microsoft' + s + version +\
                                                r'\VisualStudioLocation')
            settings_path = settings_path.replace('%VSSPV_VISUALSTUDIO_DIR%', value)
        except SCons.Util.RegError:
            raise SCons.Errors.InternalError, "The VS8 settings file location was not found in the registry."

    if settings_path.find('%') >= 0:
        # Collect global environment variables
        env_vars = {}

        # Read all the global environment variables of the current user
        k = SCons.Util.RegOpenKeyEx(SCons.Util.HKEY_CURRENT_USER, r'Environment')
        i = 0
        while 1:
            try:
                (key,val,t) = SCons.Util.RegEnumValue(k,i)
                env_vars[key.upper()] = val.upper()
                i = i + 1
            except SCons.Util.RegError:
                break

        # And some more variables that are not found in the registry
        env_vars['USERPROFILE'] = os.getenv('USERPROFILE')
        env_vars['SystemDrive'] = os.getenv('SystemDrive')

        found_var = 1
        while found_var:
            found_var = 0
            for env_var in env_vars:
                if settings_path.find(r'%' + env_var + r'%') >= 0:
                    settings_path = settings_path.replace(r'%' + env_var + r'%', env_vars[env_var])
                    found_var = 1

    dirs = {}

    if os.path.exists(settings_path):
        # now we parse the directories from this file, if it exists.
        import xml.dom.minidom
        doc = xml.dom.minidom.parse(settings_path)
        user_settings = doc.getElementsByTagName('UserSettings')[0]
        tool_options = user_settings.getElementsByTagName('ToolsOptions')[0]
        tool_options_categories = tool_options.getElementsByTagName('ToolsOptionsCategory')
        environment_var_map = {
            'IncludeDirectories' : 'INCLUDE',
            'LibraryDirectories' : 'LIBRARY',
            'ExecutableDirectories' : 'PATH',
        }
        for category in tool_options_categories:
            category_name = category.attributes.get('name')
            if category_name is not None and category_name.value == 'Projects':
                subcategories = category.getElementsByTagName('ToolsOptionsSubCategory')
                for subcategory in subcategories:
                    subcategory_name = subcategory.attributes.get('name')
                    if subcategory_name is not None and subcategory_name.value == 'VCDirectories':
                        properties = subcategory.getElementsByTagName('PropertyValue')
                        for property in properties:
                            property_name = property.attributes.get('name')
                            if property_name is None:
                                continue
                            var_name = environment_var_map.get(property_name)
                            if var_name:
                                data = property.childNodes[0].data
                                value_list = string.split(data, '|')
                                if len(value_list) == 1:
                                    dirs[var_name] = value_list[0]
                                else:
                                    while value_list:
                                        dest, value = value_list[:2]
                                        del value_list[:2]
                                        # ToDo: Support for destinations
                                        # other than Win32
                                        if dest == 'Win32':
                                            dirs[var_name] = value
                                            break
    else:
        # There are no default directories in the registry for VS8 Express :(
        raise SCons.Errors.InternalError, "Unable to find MSVC paths in the registry."
    return dirs

def _get_msvc7_path(path, version, platform):
    """
    Get Visual Studio directories from version 7 (MSVS .NET)
    (it has a different registry structure than versions before it)
    """
    # first, look for a customization of the default values in the
    # registry: These are sometimes stored in the Local Settings area
    # for Visual Studio, in a file, so we have to parse it.
    dirs = _parse_msvc7_overrides(version,platform)

    if dirs.has_key(path):
        p = dirs[path]
    else:
        raise SCons.Errors.InternalError, \
              "Unable to retrieve the %s path from MS VC++."%path

    # collect some useful information for later expansions...
    paths = SCons.Tool.msvs.get_msvs_install_dirs(version)

    # expand the directory path variables that we support.  If there
    # is a variable we don't support, then replace that entry with
    # "---Unknown Location VSInstallDir---" or something similar, to clue
    # people in that we didn't find something, and so env expansion doesn't
    # do weird things with the $(xxx)'s
    s = re.compile('\$\(([a-zA-Z0-9_]+?)\)')

    def repl(match, paths=paths):
        key = string.upper(match.group(1))
        if paths.has_key(key):
            return paths[key]
        else:
            # Now look in the global environment variables
            envresult = os.getenv(key)
            if not envresult is None:
                return envresult + '\\'
            else:
                return '---Unknown Location %s---' % match.group()

    rv = []
    for entry in p.split(os.pathsep):
        entry = s.sub(repl,entry).rstrip('\n\r')
        rv.append(entry)

    return string.join(rv,os.pathsep)

def _get_msvc8_path(path, version, platform, suite):
    """
    Get Visual Studio directories from version 8 (MSVS 2005)
    (it has a different registry structure than versions before it)
    """
    # first, look for a customization of the default values in the
    # registry: These are sometimes stored in the Local Settings area
    # for Visual Studio, in a file, so we have to parse it.
    dirs = _parse_msvc8_overrides(version, platform, suite)

    if dirs.has_key(path):
        p = dirs[path]
    else:
        raise SCons.Errors.InternalError, \
              "Unable to retrieve the %s path from MS VC++."%path

    # collect some useful information for later expansions...
    paths = SCons.Tool.msvs.get_msvs_install_dirs(version, suite)

    # expand the directory path variables that we support.  If there
    # is a variable we don't support, then replace that entry with
    # "---Unknown Location VSInstallDir---" or something similar, to clue
    # people in that we didn't find something, and so env expansion doesn't
    # do weird things with the $(xxx)'s
    s = re.compile('\$\(([a-zA-Z0-9_]+?)\)')

    def repl(match, paths=paths):
        key = string.upper(match.group(1))
        if paths.has_key(key):
            return paths[key]
        else:
            return '---Unknown Location %s---' % match.group()

    rv = []
    for entry in p.split(os.pathsep):
        entry = s.sub(repl,entry).rstrip('\n\r')
        rv.append(entry)

    return string.join(rv,os.pathsep)

def get_msvc_path(env, path, version):
    """
    Get a list of visualstudio directories (include, lib or path).
    Return a string delimited by the os.pathsep separator (';'). An
    exception will be raised if unable to access the registry or
    appropriate registry keys not found.
    """

    if not SCons.Util.can_read_reg:
        raise SCons.Errors.InternalError, "No Windows registry module was found"

    # normalize the case for comparisons (since the registry is case
    # insensitive)
    path = string.upper(path)

    if path=='LIB':
        path= 'LIBRARY'

    version_num, suite = SCons.Tool.msvs.msvs_parse_version(version)
    if version_num >= 8.0:
        platform = env.get('MSVS8_PLATFORM', 'x86')
        suite = SCons.Tool.msvs.get_default_visualstudio8_suite(env)
    else:
        platform = 'x86'

    if version_num >= 8.0:
        return _get_msvc8_path(path, str(version_num), platform, suite)
    elif version_num >= 7.0:
        return _get_msvc7_path(path, str(version_num), platform)

    path = string.upper(path + ' Dirs')
    K = ('Software\\Microsoft\\Devstudio\\%s\\' +
         'Build System\\Components\\Platforms\\Win32 (x86)\\Directories') % \
        (version)
    for base in (SCons.Util.HKEY_CURRENT_USER,
                 SCons.Util.HKEY_LOCAL_MACHINE):
        try:
            k = SCons.Util.RegOpenKeyEx(base,K)
            i = 0
            while 1:
                try:
                    (p,v,t) = SCons.Util.RegEnumValue(k,i)
                    if string.upper(p) == path:
                        return v
                    i = i + 1
                except SCons.Util.RegError:
                    break
        except SCons.Util.RegError:
            pass

    # if we got here, then we didn't find the registry entries:
    raise SCons.Errors.InternalError, "The %s path was not found in the registry."%path

def _get_msvc6_default_paths(version, use_mfc_dirs):
    """Return a 3-tuple of (INCLUDE, LIB, PATH) as the values of those
    three environment variables that should be set in order to execute
    the MSVC 6.0 tools properly, if the information wasn't available
    from the registry."""
    MVSdir = None
    paths = {}
    exe_path = ''
    lib_path = ''
    include_path = ''
    try:
        paths = SCons.Tool.msvs.get_msvs_install_dirs(version)
        MVSdir = paths['VSINSTALLDIR']
    except (SCons.Util.RegError, SCons.Errors.InternalError, KeyError):
        if os.environ.has_key('MSDEVDIR'):
            MVSdir = os.path.normpath(os.path.join(os.environ['MSDEVDIR'],'..','..'))
        else:
            MVSdir = r'C:\Program Files\Microsoft Visual Studio'
    if MVSdir:
        if SCons.Util.can_read_reg and paths.has_key('VCINSTALLDIR'):
            MVSVCdir = paths['VCINSTALLDIR']
        else:
            MVSVCdir = os.path.join(MVSdir,'VC98')

        MVSCommondir = r'%s\Common' % MVSdir
        if use_mfc_dirs:
            mfc_include_ = r'%s\ATL\include;%s\MFC\include;' % (MVSVCdir, MVSVCdir)
            mfc_lib_ = r'%s\MFC\lib;' % MVSVCdir
        else:
            mfc_include_ = ''
            mfc_lib_ = ''
        include_path = r'%s%s\include' % (mfc_include_, MVSVCdir)
        lib_path = r'%s%s\lib' % (mfc_lib_, MVSVCdir)

        if os.environ.has_key('OS') and os.environ['OS'] == "Windows_NT":
            osdir = 'WINNT'
        else:
            osdir = 'WIN95'

        exe_path = r'%s\tools\%s;%s\MSDev98\bin;%s\tools;%s\bin' % (MVSCommondir, osdir, MVSCommondir,  MVSCommondir, MVSVCdir)
    return (include_path, lib_path, exe_path)

def _get_msvc7_default_paths(env, version, use_mfc_dirs):
    """Return a 3-tuple of (INCLUDE, LIB, PATH) as the values of those
    three environment variables that should be set in order to execute
    the MSVC .NET tools properly, if the information wasn't available
    from the registry."""

    MVSdir = None
    paths = {}
    exe_path = ''
    lib_path = ''
    include_path = ''
    try:
        paths = SCons.Tool.msvs.get_msvs_install_dirs(version)
        MVSdir = paths['VSINSTALLDIR']
    except (KeyError, SCons.Util.RegError, SCons.Errors.InternalError):
        if os.environ.has_key('VSCOMNTOOLS'):
            MVSdir = os.path.normpath(os.path.join(os.environ['VSCOMNTOOLS'],'..','..'))
        else:
            # last resort -- default install location
            MVSdir = r'C:\Program Files\Microsoft Visual Studio .NET'

    if MVSdir:
        if SCons.Util.can_read_reg and paths.has_key('VCINSTALLDIR'):
            MVSVCdir = paths['VCINSTALLDIR']
        else:
            MVSVCdir = os.path.join(MVSdir,'Vc7')

        MVSCommondir = r'%s\Common7' % MVSdir
        if use_mfc_dirs:
            mfc_include_ = r'%s\atlmfc\include;' % MVSVCdir
            mfc_lib_ = r'%s\atlmfc\lib;' % MVSVCdir
        else:
            mfc_include_ = ''
            mfc_lib_ = ''
        include_path = r'%s%s\include;%s\PlatformSDK\include' % (mfc_include_, MVSVCdir, MVSVCdir)
        lib_path = r'%s%s\lib;%s\PlatformSDK\lib' % (mfc_lib_, MVSVCdir, MVSVCdir)
        exe_path = r'%s\IDE;%s\bin;%s\Tools;%s\Tools\bin' % (MVSCommondir,MVSVCdir, MVSCommondir, MVSCommondir )

        if SCons.Util.can_read_reg and paths.has_key('FRAMEWORKSDKDIR'):
            include_path = include_path + r';%s\include'%paths['FRAMEWORKSDKDIR']
            lib_path = lib_path + r';%s\lib'%paths['FRAMEWORKSDKDIR']
            exe_path = exe_path + r';%s\bin'%paths['FRAMEWORKSDKDIR']

        if SCons.Util.can_read_reg and paths.has_key('FRAMEWORKDIR') and paths.has_key('FRAMEWORKVERSION'):
            exe_path = exe_path + r';%s\%s'%(paths['FRAMEWORKDIR'],paths['FRAMEWORKVERSION'])

    return (include_path, lib_path, exe_path)

def _get_msvc8_default_paths(env, version, suite, use_mfc_dirs):
    """Return a 3-tuple of (INCLUDE, LIB, PATH) as the values of those
    three environment variables that should be set in order to execute
    the MSVC 8 tools properly, if the information wasn't available
    from the registry."""

    MVSdir = None
    paths = {}
    exe_paths = []
    lib_paths = []
    include_paths = []
    try:
        paths = SCons.Tool.msvs.get_msvs_install_dirs(version, suite)
        MVSdir = paths['VSINSTALLDIR']
    except (KeyError, SCons.Util.RegError, SCons.Errors.InternalError):
        if os.environ.has_key('VSCOMNTOOLS'):
            MVSdir = os.path.normpath(os.path.join(os.environ['VSCOMNTOOLS'],'..','..'))
        else:
            # last resort -- default install location
            MVSdir = os.getenv('ProgramFiles') + r'\Microsoft Visual Studio 8'

    if MVSdir:
        if SCons.Util.can_read_reg and paths.has_key('VCINSTALLDIR'):
            MVSVCdir = paths['VCINSTALLDIR']
        else:
            MVSVCdir = os.path.join(MVSdir,'VC')

        MVSCommondir = os.path.join(MVSdir, 'Common7')
        include_paths.append( os.path.join(MVSVCdir, 'include') )
        lib_paths.append( os.path.join(MVSVCdir, 'lib') )
        for base, subdir in [(MVSCommondir,'IDE'), (MVSVCdir,'bin'),
                             (MVSCommondir,'Tools'), (MVSCommondir,r'Tools\bin')]:
            exe_paths.append( os.path.join( base, subdir) )

        if paths.has_key('PLATFORMSDKDIR'):
            PlatformSdkDir = paths['PLATFORMSDKDIR']
        else:
            PlatformSdkDir = os.path.join(MVSVCdir,'PlatformSDK')
        platform_include_path = os.path.join( PlatformSdkDir, 'Include' )
        include_paths.append( platform_include_path )
        lib_paths.append( os.path.join( PlatformSdkDir, 'Lib' ) )
        if use_mfc_dirs:
            if paths.has_key('PLATFORMSDKDIR'):
                include_paths.append( os.path.join( platform_include_path, 'mfc' ) )
                include_paths.append( os.path.join( platform_include_path, 'atl' ) )
            else:
                atlmfc_path = os.path.join( MVSVCdir, 'atlmfc' )
                include_paths.append( os.path.join( atlmfc_path, 'include' ) )
                lib_paths.append( os.path.join( atlmfc_path, 'lib' ) )

        if SCons.Util.can_read_reg and paths.has_key('FRAMEWORKSDKDIR'):
            fwdir = paths['FRAMEWORKSDKDIR']
            include_paths.append( os.path.join( fwdir, 'include' ) )
            lib_paths.append( os.path.join( fwdir, 'lib' ) )
            exe_paths.append( os.path.join( fwdir, 'bin' ) )

        if SCons.Util.can_read_reg and paths.has_key('FRAMEWORKDIR') and paths.has_key('FRAMEWORKVERSION'):
            exe_paths.append( os.path.join( paths['FRAMEWORKDIR'], paths['FRAMEWORKVERSION'] ) )

    include_path = string.join( include_paths, os.pathsep )
    lib_path = string.join(lib_paths, os.pathsep )
    exe_path = string.join(exe_paths, os.pathsep )
    return (include_path, lib_path, exe_path)

def get_msvc_paths(env, version=None, use_mfc_dirs=0):
    """Return a 3-tuple of (INCLUDE, LIB, PATH) as the values
    of those three environment variables that should be set
    in order to execute the MSVC tools properly."""
    exe_path = ''
    lib_path = ''
    include_path = ''

    if not version:
        versions = SCons.Tool.msvs.get_visualstudio_versions()
        if versions:
            version = versions[0] #use highest version by default
        else:
            version = '6.0'

    # Some of the configured directories only
    # appear if the user changes them from the default.
    # Therefore, we'll see if we can get the path to the MSDev
    # base installation from the registry and deduce the default
    # directories.
    version_num, suite = SCons.Tool.msvs.msvs_parse_version(version)
    if version_num >= 8.0:
        suite = SCons.Tool.msvs.get_default_visualstudio8_suite(env)
        defpaths = _get_msvc8_default_paths(env, version, suite, use_mfc_dirs)
    elif version_num >= 7.0:
        defpaths = _get_msvc7_default_paths(env, version, use_mfc_dirs)
    else:
        defpaths = _get_msvc6_default_paths(version, use_mfc_dirs)

    try:
        include_path = get_msvc_path(env, "include", version)
    except (SCons.Util.RegError, SCons.Errors.InternalError):
        include_path = defpaths[0]

    try:
        lib_path = get_msvc_path(env, "lib", version)
    except (SCons.Util.RegError, SCons.Errors.InternalError):
        lib_path = defpaths[1]

    try:
        exe_path = get_msvc_path(env, "path", version)
    except (SCons.Util.RegError, SCons.Errors.InternalError):
        exe_path = defpaths[2]

    return (include_path, lib_path, exe_path)

def get_msvc_default_paths(env, version=None, use_mfc_dirs=0):
    """Return a 3-tuple of (INCLUDE, LIB, PATH) as the values of those
    three environment variables that should be set in order to execute
    the MSVC tools properly.  This will only return the default
    locations for the tools, not the values used by MSVS in their
    directory setup area.  This can help avoid problems with different
    developers having different settings, and should allow the tools
    to run in most cases."""

    if not version and not SCons.Util.can_read_reg:
        version = '6.0'

    try:
        if not version:
            version = SCons.Tool.msvs.get_visualstudio_versions()[0] #use highest version
    except KeyboardInterrupt:
        raise
    except:
        pass

    version_num, suite = SCons.Tool.msvs.msvs_parse_version(version)
    if version_num >= 8.0:
        suite = SCons.Tool.msvs.get_default_visualstudio8_suite(env)
        return _get_msvc8_default_paths(env, version, suite, use_mfc_dirs)
    elif version_num >= 7.0:
        return _get_msvc7_default_paths(env, version, use_mfc_dirs)
    else:
        return _get_msvc6_default_paths(version, use_mfc_dirs)

def validate_vars(env):
    """Validate the PCH and PCHSTOP construction variables."""
    if env.has_key('PCH') and env['PCH']:
        if not env.has_key('PCHSTOP'):
            raise SCons.Errors.UserError, "The PCHSTOP construction must be defined if PCH is defined."
        if not SCons.Util.is_String(env['PCHSTOP']):
            raise SCons.Errors.UserError, "The PCHSTOP construction variable must be a string: %r"%env['PCHSTOP']

def pch_emitter(target, source, env):
    """Adds the object file target."""

    validate_vars(env)

    pch = None
    obj = None

    for t in target:
        if SCons.Util.splitext(str(t))[1] == '.pch':
            pch = t
        if SCons.Util.splitext(str(t))[1] == '.obj':
            obj = t

    if not obj:
        obj = SCons.Util.splitext(str(pch))[0]+'.obj'

    target = [pch, obj] # pch must be first, and obj second for the PCHCOM to work

    return (target, source)

def object_emitter(target, source, env, parent_emitter):
    """Sets up the PCH dependencies for an object file."""

    validate_vars(env)

    parent_emitter(target, source, env)

    if env.has_key('PCH') and env['PCH']:
        env.Depends(target, env['PCH'])

    return (target, source)

def static_object_emitter(target, source, env):
    return object_emitter(target, source, env,
                          SCons.Defaults.StaticObjectEmitter)

def shared_object_emitter(target, source, env):
    return object_emitter(target, source, env,
                          SCons.Defaults.SharedObjectEmitter)

pch_action = SCons.Action.Action('$PCHCOM', '$PCHCOMSTR')
pch_builder = SCons.Builder.Builder(action=pch_action, suffix='.pch',
                                    emitter=pch_emitter,
                                    source_scanner=SCons.Tool.SourceFileScanner)


# Logic to build .rc files into .res files (resource files)
res_scanner = SCons.Scanner.RC.RCScan()
res_action  = SCons.Action.Action('$RCCOM', '$RCCOMSTR')
res_builder = SCons.Builder.Builder(action=res_action,
                                    src_suffix='.rc',
                                    suffix='.res',
                                    src_builder=[],
                                    source_scanner=res_scanner)


def generate(env):
    """Add Builders and construction variables for MSVC++ to an Environment."""
    static_obj, shared_obj = SCons.Tool.createObjBuilders(env)

    for suffix in CSuffixes:
        static_obj.add_action(suffix, SCons.Defaults.CAction)
        shared_obj.add_action(suffix, SCons.Defaults.ShCAction)
        static_obj.add_emitter(suffix, static_object_emitter)
        shared_obj.add_emitter(suffix, shared_object_emitter)

    for suffix in CXXSuffixes:
        static_obj.add_action(suffix, SCons.Defaults.CXXAction)
        shared_obj.add_action(suffix, SCons.Defaults.ShCXXAction)
        static_obj.add_emitter(suffix, static_object_emitter)
        shared_obj.add_emitter(suffix, shared_object_emitter)

    env['CCPDBFLAGS'] = SCons.Util.CLVar(['${(PDB and "/Z7") or ""}'])
    env['CCPCHFLAGS'] = SCons.Util.CLVar(['${(PCH and "/Yu%s /Fp%s"%(PCHSTOP or "",File(PCH))) or ""}'])
    env['_CCCOMCOM']  = '$CPPFLAGS $_CPPDEFFLAGS $_CPPINCFLAGS $CCPCHFLAGS $CCPDBFLAGS'
    env['CC']         = 'cl'
    env['CCFLAGS']    = SCons.Util.CLVar('/nologo')
    env['CFLAGS']     = SCons.Util.CLVar('')
    env['CCCOM']      = '$CC /Fo$TARGET /c $SOURCES $CFLAGS $CCFLAGS $_CCCOMCOM'
    env['SHCC']       = '$CC'
    env['SHCCFLAGS']  = SCons.Util.CLVar('$CCFLAGS')
    env['SHCFLAGS']   = SCons.Util.CLVar('$CFLAGS')
    env['SHCCCOM']    = '$SHCC /Fo$TARGET /c $SOURCES $SHCFLAGS $SHCCFLAGS $_CCCOMCOM'
    env['CXX']        = '$CC'
    env['CXXFLAGS']   = SCons.Util.CLVar('$CCFLAGS $( /TP $)')
    env['CXXCOM']     = '$CXX /Fo$TARGET /c $SOURCES $CXXFLAGS $CCFLAGS $_CCCOMCOM'
    env['SHCXX']      = '$CXX'
    env['SHCXXFLAGS'] = SCons.Util.CLVar('$CXXFLAGS')
    env['SHCXXCOM']   = '$SHCXX /Fo$TARGET /c $SOURCES $SHCXXFLAGS $SHCCFLAGS $_CCCOMCOM'
    env['CPPDEFPREFIX']  = '/D'
    env['CPPDEFSUFFIX']  = ''
    env['INCPREFIX']  = '/I'
    env['INCSUFFIX']  = ''
#    env.Append(OBJEMITTER = [static_object_emitter])
#    env.Append(SHOBJEMITTER = [shared_object_emitter])
    env['STATIC_AND_SHARED_OBJECTS_ARE_THE_SAME'] = 1

    env['RC'] = 'rc'
    env['RCFLAGS'] = SCons.Util.CLVar('')
    env['RCSUFFIXES']=['.rc','.rc2']
    env['RCCOM'] = '$RC $_CPPDEFFLAGS $_CPPINCFLAGS $RCFLAGS /fo$TARGET $SOURCES'
    env['BUILDERS']['RES'] = res_builder
    env['OBJPREFIX']      = ''
    env['OBJSUFFIX']      = '.obj'
    env['SHOBJPREFIX']    = '$OBJPREFIX'
    env['SHOBJSUFFIX']    = '$OBJSUFFIX'

    try:
        version = SCons.Tool.msvs.get_default_visualstudio_version(env)
        version_num, suite = SCons.Tool.msvs.msvs_parse_version(version)
        if version_num == 8.0:
            suite = SCons.Tool.msvs.get_default_visualstudio8_suite(env)

        use_mfc_dirs = env.get('MSVS_USE_MFC_DIRS', 0)
        if env.get('MSVS_IGNORE_IDE_PATHS', 0):
            _get_paths = get_msvc_default_paths
        else:
            _get_paths = get_msvc_paths
        include_path, lib_path, exe_path = _get_paths(env, version, use_mfc_dirs)

        # since other tools can set these, we just make sure that the
        # relevant stuff from MSVS is in there somewhere.
        env.PrependENVPath('INCLUDE', include_path)
        env.PrependENVPath('LIB', lib_path)
        env.PrependENVPath('PATH', exe_path)
    except (SCons.Util.RegError, SCons.Errors.InternalError):
        pass

    env['CFILESUFFIX'] = '.c'
    env['CXXFILESUFFIX'] = '.cc'

    env['PCHPDBFLAGS'] = SCons.Util.CLVar(['${(PDB and "/Yd") or ""}'])
    env['PCHCOM'] = '$CXX $CXXFLAGS $CPPFLAGS $_CPPDEFFLAGS $_CPPINCFLAGS /c $SOURCES /Fo${TARGETS[1]} /Yc$PCHSTOP /Fp${TARGETS[0]} $CCPDBFLAGS $PCHPDBFLAGS'
    env['BUILDERS']['PCH'] = pch_builder

    if not env.has_key('ENV'):
        env['ENV'] = {}
    if not env['ENV'].has_key('SystemRoot'):    # required for dlls in the winsxs folders
        env['ENV']['SystemRoot'] = SCons.Platform.win32.get_system_root()

def exists(env):
    if SCons.Tool.msvs.is_msvs_installed():
        # there's at least one version of MSVS installed.
        return 1
    else:
        return env.Detect('cl')

