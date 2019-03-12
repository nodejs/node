# Copyright (c) 2013 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Handle version information related to Visual Stdio."""

import os
import glob
from gyp.MSVS import TryQueryRegistryValue

msvs_version_map = {
  'auto': ('16.0', '15.0', '14.0', '12.0', '10.0', '9.0', '8.0', '11.0'),
  '2005': ('8.0',),
  '2005e': ('8.0',),
  '2008': ('9.0',),
  '2008e': ('9.0',),
  '2010': ('10.0',),
  '2010e': ('10.0',),
  '2012': ('11.0',),
  '2012e': ('11.0',),
  '2013': ('12.0',),
  '2013e': ('12.0',),
  '2015': ('14.0',),
  '2017': ('15.0',),
  '2019': ('16.0',),
}
version_to_year = {
  '8.0': '2005',
  '9.0': '2008',
  '10.0': '2010',
  '11.0': '2012',
  '12.0': '2013',
  '14.0': '2015',
  '15.0': '2017',
  '16.0': '2019',
}

def _JoinPath(*args):
  return os.path.normpath(os.path.join(*args))


class VisualStudioVersion(object):
  """Information regarding a version of Visual Studio."""

  def __init__(self,
               short_name, description,
               solution_version, project_version,
               flat_sln=False, uses_vcxproj=True,
               default_toolset=None, compatible_sdks=None):
    self.short_name = short_name
    self.description = description
    self.solution_version = solution_version
    self.project_version = project_version
    self.flat_sln = flat_sln
    self.uses_vcxproj = uses_vcxproj
    self.default_toolset = default_toolset
    self.compatible_sdks = compatible_sdks
    self.path = ''
    self.sdk_based = False

  def ProjectExtension(self):
    """Returns the file extension for the project."""
    return self.uses_vcxproj and '.vcxproj' or '.vcproj'

  def ToolPath(self, tool):
    """Returns the path to a given compiler tool. """
    return os.path.normpath(os.path.join(self.path, "VC", "bin", tool))

  def DefaultToolset(self):
    """Returns the msbuild toolset version that will be used in the absence
    of a user override."""
    return self.default_toolset

  def _SetupScriptInternal(self, target_arch):
    """Returns a command (with arguments) to be used to set up the
    environment."""
    # If WindowsSDKDir is set and SetEnv.Cmd exists then we are using the
    # depot_tools build tools and should run SetEnv.Cmd to set up the
    # environment. The check for WindowsSDKDir alone is not sufficient because
    # this is set by running vcvarsall.bat.
    sdk_dir = os.environ.get('WindowsSDKDir', '')
    setup_path = _JoinPath(sdk_dir, 'Bin', 'SetEnv.Cmd')
    if self.sdk_based and sdk_dir and os.path.exists(setup_path):
      return [setup_path, '/' + target_arch]

    is_host_arch_x64 = (
      os.environ.get('PROCESSOR_ARCHITECTURE') == 'AMD64' or
      os.environ.get('PROCESSOR_ARCHITEW6432') == 'AMD64'
    )

    # For VS2017 (and newer) it's fairly easy
    if self.short_name >= '2017':
      script_path = _JoinPath(self.path, 'VC', 'Auxiliary', 'Build', 'vcvarsall.bat')

      # Always use a native executable, cross-compiling if necessary.
      host_arch = 'amd64' if is_host_arch_x64 else 'x86'
      msvc_target_arch = 'amd64' if target_arch == 'x64' else 'x86'
      arg = host_arch
      if host_arch != msvc_target_arch:
        arg += '_' + msvc_target_arch

      return [script_path, arg]

    # We try to find the best version of the env setup batch.
    vcvarsall = _JoinPath(self.path, 'VC', 'vcvarsall.bat')
    if target_arch == 'x86':
      if self.short_name >= '2013' and self.short_name[-1] != 'e' and is_host_arch_x64:
        # VS2013 and later, non-Express have a x64-x86 cross that we want
        # to prefer.
        return [vcvarsall, 'amd64_x86']
      else:
        # Otherwise, the standard x86 compiler. We don't use VC/vcvarsall.bat
        # for x86 because vcvarsall calls vcvars32, which it can only find if
        # VS??COMNTOOLS is set, which isn't guaranteed.
        return [_JoinPath(self.path, 'Common7', 'Tools', 'vsvars32.bat')]
    elif target_arch == 'x64':
      arg = 'x86_amd64'
      # Use the 64-on-64 compiler if we're not using an express edition and
      # we're running on a 64bit OS.
      if self.short_name[-1] != 'e' and is_host_arch_x64:
        arg = 'amd64'
      return [vcvarsall, arg]

  def SetupScript(self, target_arch):
    script_data = self._SetupScriptInternal(target_arch)
    script_path = script_data[0]
    if not os.path.exists(script_path):
      raise Exception('%s is missing - make sure VC++ tools are installed.' % script_path)
    return script_data


MSVS_VERSIONS = {
  '2019':
    VisualStudioVersion(
      short_name='2019',
      description='Visual Studio 2019',
      solution_version='12.00',
      project_version='15.0',
      default_toolset='v142',
      compatible_sdks='8.1,10.0'
    ),
  '2017':
    VisualStudioVersion(
      short_name='2017',
      description='Visual Studio 2017',
      solution_version='12.00',
      project_version='15.0',
      default_toolset='v141',
      compatible_sdks='8.1,10.0'
    ),
  '2015':
    VisualStudioVersion(
      short_name='2015',
      description='Visual Studio 2015',
      solution_version='12.00',
      project_version='14.0',
      default_toolset='v140'
    ),
  '2013':
    VisualStudioVersion(
      short_name='2013',
      description='Visual Studio 2013',
      solution_version='13.00',
      project_version='12.0',
      default_toolset='v120'
    ),
  '2013e':
    VisualStudioVersion(
      short_name='2013e',
      description='Visual Studio 2013',
      solution_version='13.00',
      project_version='12.0',
      default_toolset='v120'
    ),
  '2012':
    VisualStudioVersion(
      short_name='2012',
      description='Visual Studio 2012',
      solution_version='12.00',
      project_version='4.0',
      default_toolset='v110'
    ),
  '2012e':
    VisualStudioVersion(
      short_name='2012e',
      description='Visual Studio 2012',
      solution_version='12.00',
      project_version='4.0',
      flat_sln=True,
      default_toolset='v110'
    ),
  '2010':
    VisualStudioVersion(
      short_name='2010',
      description='Visual Studio 2010',
      solution_version='11.00',
      project_version='4.0',
    ),
  '2010e':
    VisualStudioVersion(
      short_name='2010e',
      description='Visual C++ Express 2010',
      solution_version='11.00',
      project_version='4.0',
      flat_sln=True,
    ),
  '2008':
    VisualStudioVersion(
      short_name='2008',
      description='Visual Studio 2008',
      solution_version='10.00',
      project_version='9.00',
      uses_vcxproj=False,
    ),
  '2008e':
    VisualStudioVersion(
      short_name='2008e',
      description='Visual Studio 2008',
      solution_version='10.00',
      project_version='9.00',
      flat_sln=True,
      uses_vcxproj=False,
    ),
  '2005':
    VisualStudioVersion(
      short_name='2005',
      description='Visual Studio 2005',
      solution_version='9.00',
      project_version='8.00',
      flat_sln=False,
      uses_vcxproj=False,
    ),
  '2005e':
    VisualStudioVersion(
      short_name='2005e',
      description='Visual Studio 2005',
      solution_version='9.00',
      project_version='8.00',
      flat_sln=True,
      uses_vcxproj=False,
    ),
}

def _CreateVersion(name, path, sdk_based=False):
  """
  Sets up MSVS project generation.

  Setup is based off the GYP_MSVS_VERSION environment variable or whatever is auto-detected if GYP_MSVS_VERSION
  is not explicitly specified. If a version is passed in that doesn't match a value in versions python will throw a error.
  """
  if path:
    path = os.path.normpath(path)
  version = MSVS_VERSIONS[str(name)]
  version.path = path
  version.sdk_based = sdk_based
  return version


def _DetectVisualStudioVersion(wanted_version, force_express):
  for version in msvs_version_map[wanted_version]:
    # Old method of searching for which VS version is installed
    # We don't use the 2010-encouraged-way because we also want to get the
    # path to the binaries, which it doesn't offer.
    keys = [
      r'Software\Microsoft\VisualStudio\%s' % version,
      r'Software\Wow6432Node\Microsoft\VisualStudio\%s' % version,
      r'Software\Microsoft\VCExpress\%s' % version,
      r'Software\Wow6432Node\Microsoft\VCExpress\%s' % version
    ]
    for key in keys:
      path = TryQueryRegistryValue(key, 'InstallDir')
      if not path:
        continue
      # Check for full.
      full_path = os.path.join(path, 'devenv.exe')
      express_path = os.path.join(path, '*express.exe')
      vc_root_path = os.path.join(path, '..', '..')
      if not force_express and os.path.exists(full_path):
        # Add this one.
        return _CreateVersion(version_to_year[version], vc_root_path)
      # Check for express.
      elif glob.glob(express_path):
        # Add this one.
        return _CreateVersion(version_to_year[version] + 'e', vc_root_path)

    # The old method above does not work when only SDK is installed.
    keys2 = [
      r'Software\Microsoft\VisualStudio\SxS\VC7',
      r'Software\Wow6432Node\Microsoft\VisualStudio\SxS\VC7',
      r'Software\Microsoft\VisualStudio\SxS\VS7',
      r'Software\Wow6432Node\Microsoft\VisualStudio\SxS\VS7'
    ]
    for key in keys2:
      path = TryQueryRegistryValue(key, version)
      if not path:
        continue
      if version == '15.0':
        if os.path.exists(path):
          return _CreateVersion('2017', path)
      elif version == '16.0':
        if os.path.exists(path):
          return _CreateVersion('2019', path)
      elif version != '14.0':  # There is no Express edition for 2015.
        return _CreateVersion(version_to_year[version] + 'e', os.path.join(path, '..'), sdk_based=True)

  return None


def SelectVisualStudioVersion(wanted_version='auto'):
  """Select which version of Visual Studio projects to generate.

  Arguments:
    wanted_version: Hook to allow caller to force a particular version (vs auto).
  Returns:
    An object representing a visual studio project format version.
  """
  wanted_version = str(wanted_version)

  override_path = os.environ.get('GYP_MSVS_OVERRIDE_PATH')
  gyp_env_version = os.environ.get('GYP_MSVS_VERSION', wanted_version)
  if override_path:
    if gyp_env_version == 'auto':
      raise ValueError('GYP_MSVS_OVERRIDE_PATH requires GYP_MSVS_VERSION to be set to a particular version.')
    return _CreateVersion(gyp_env_version, override_path, sdk_based=True)

  # In auto mode, check environment variable for override.
  if wanted_version == 'auto':
    wanted_version = version_to_year.get(os.environ.get('VisualStudioVersion'), gyp_env_version)
  version = _DetectVisualStudioVersion(wanted_version, 'e' in wanted_version)
  if version:
    return version
  if wanted_version != 'auto':
    # Even if we did not actually detect a version, we fake it
    return _CreateVersion(wanted_version, None)
  raise ValueError('Could not locate Visual Studio installation.')


def WindowsTargetPlatformVersion(possible_sdk_versions):
  # If the environment is set, ignore the possible version hint
  env_sdk_version = os.environ.get('WindowsSDKVersion', '')
  if env_sdk_version:
    return env_sdk_version.replace('\\', '')

  if not possible_sdk_versions:
    return None

  versions_args = possible_sdk_versions.split(',')
  key_template = r'Software\%sMicrosoft\Microsoft SDKs\Windows\%s'
  keys = [(key_template % (sub, ver)) for ver in versions_args for sub in ['', 'Wow6432Node\\',]]
  for key in keys:
    sdk_dir = TryQueryRegistryValue(key, 'InstallationFolder')
    if not sdk_dir:
      continue
    # Find a matching entry in sdk_dir\include.
    product_version = TryQueryRegistryValue(key, 'ProductVersion')
    sdk_include_dir = os.path.join(sdk_dir, 'include')
    if not os.path.isdir(sdk_include_dir):
      continue
    names = sorted(x for x in os.listdir(sdk_include_dir) if x.startswith(product_version))
    if names:
      return names[-1]

  return None


