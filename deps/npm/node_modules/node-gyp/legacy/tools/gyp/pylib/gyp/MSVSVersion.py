# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Handle version information related to Visual Stuio."""

import errno
import os
import re
import subprocess
import sys


class VisualStudioVersion(object):
  """Information regarding a version of Visual Studio."""

  def __init__(self, short_name, description,
               solution_version, project_version, flat_sln, uses_vcxproj):
    self.short_name = short_name
    self.description = description
    self.solution_version = solution_version
    self.project_version = project_version
    self.flat_sln = flat_sln
    self.uses_vcxproj = uses_vcxproj

  def ShortName(self):
    return self.short_name

  def Description(self):
    """Get the full description of the version."""
    return self.description

  def SolutionVersion(self):
    """Get the version number of the sln files."""
    return self.solution_version

  def ProjectVersion(self):
    """Get the version number of the vcproj or vcxproj files."""
    return self.project_version

  def FlatSolution(self):
    return self.flat_sln

  def UsesVcxproj(self):
    """Returns true if this version uses a vcxproj file."""
    return self.uses_vcxproj

  def ProjectExtension(self):
    """Returns the file extension for the project."""
    return self.uses_vcxproj and '.vcxproj' or '.vcproj'

def _RegistryQueryBase(sysdir, key, value):
  """Use reg.exe to read a particular key.

  While ideally we might use the win32 module, we would like gyp to be
  python neutral, so for instance cygwin python lacks this module.

  Arguments:
    sysdir: The system subdirectory to attempt to launch reg.exe from.
    key: The registry key to read from.
    value: The particular value to read.
  Return:
    stdout from reg.exe, or None for failure.
  """
  # Skip if not on Windows or Python Win32 setup issue
  if sys.platform not in ('win32', 'cygwin'):
    return None
  # Setup params to pass to and attempt to launch reg.exe
  cmd = [os.path.join(os.environ.get('WINDIR', ''), sysdir, 'reg.exe'),
         'query', key]
  if value:
    cmd.extend(['/v', value])
  p = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
  # Obtain the stdout from reg.exe, reading to the end so p.returncode is valid
  # Note that the error text may be in [1] in some cases
  text = p.communicate()[0]
  # Check return code from reg.exe; officially 0==success and 1==error
  if p.returncode:
    return None
  return text


def _RegistryQuery(key, value=None):
  """Use reg.exe to read a particular key through _RegistryQueryBase.

  First tries to launch from %WinDir%\Sysnative to avoid WoW64 redirection. If
  that fails, it falls back to System32.  Sysnative is available on Vista and
  up and available on Windows Server 2003 and XP through KB patch 942589. Note
  that Sysnative will always fail if using 64-bit python due to it being a
  virtual directory and System32 will work correctly in the first place.

  KB 942589 - http://support.microsoft.com/kb/942589/en-us.

  Arguments:
    key: The registry key.
    value: The particular registry value to read (optional).
  Return:
    stdout from reg.exe, or None for failure.
  """
  text = None
  try:
    text = _RegistryQueryBase('Sysnative', key, value)
  except OSError, e:
    if e.errno == errno.ENOENT:
      text = _RegistryQueryBase('System32', key, value)
    else:
      raise
  return text


def _RegistryGetValue(key, value):
  """Use reg.exe to obtain the value of a registry key.

  Args:
    key: The registry key.
    value: The particular registry value to read.
  Return:
    contents of the registry key's value, or None on failure.
  """
  text = _RegistryQuery(key, value)
  if not text:
    return None
  # Extract value.
  match = re.search(r'REG_\w+\s+([^\r]+)\r\n', text)
  if not match:
    return None
  return match.group(1)


def _RegistryKeyExists(key):
  """Use reg.exe to see if a key exists.

  Args:
    key: The registry key to check.
  Return:
    True if the key exists
  """
  if not _RegistryQuery(key):
    return False
  return True


def _CreateVersion(name):
  """Sets up MSVS project generation.

  Setup is based off the GYP_MSVS_VERSION environment variable or whatever is
  autodetected if GYP_MSVS_VERSION is not explicitly specified. If a version is
  passed in that doesn't match a value in versions python will throw a error.
  """
  versions = {
      '2010': VisualStudioVersion('2010',
                                  'Visual Studio 2010',
                                  solution_version='11.00',
                                  project_version='4.0',
                                  flat_sln=False,
                                  uses_vcxproj=True),
      '2010e': VisualStudioVersion('2010e',
                                   'Visual Studio 2010',
                                   solution_version='11.00',
                                   project_version='4.0',
                                   flat_sln=True,
                                   uses_vcxproj=True),
      '2008': VisualStudioVersion('2008',
                                  'Visual Studio 2008',
                                  solution_version='10.00',
                                  project_version='9.00',
                                  flat_sln=False,
                                  uses_vcxproj=False),
      '2008e': VisualStudioVersion('2008e',
                                   'Visual Studio 2008',
                                   solution_version='10.00',
                                   project_version='9.00',
                                   flat_sln=True,
                                   uses_vcxproj=False),
      '2005': VisualStudioVersion('2005',
                                  'Visual Studio 2005',
                                  solution_version='9.00',
                                  project_version='8.00',
                                  flat_sln=False,
                                  uses_vcxproj=False),
      '2005e': VisualStudioVersion('2005e',
                                   'Visual Studio 2005',
                                   solution_version='9.00',
                                   project_version='8.00',
                                   flat_sln=True,
                                   uses_vcxproj=False),
  }
  return versions[str(name)]


def _DetectVisualStudioVersions():
  """Collect the list of installed visual studio versions.

  Returns:
    A list of visual studio versions installed in descending order of
    usage preference.
    Base this on the registry and a quick check if devenv.exe exists.
    Only versions 8-10 are considered.
    Possibilities are:
      2005(e) - Visual Studio 2005 (8)
      2008(e) - Visual Studio 2008 (9)
      2010(e) - Visual Studio 2010 (10)
    Where (e) is e for express editions of MSVS and blank otherwise.
  """
  version_to_year = {'8.0': '2005', '9.0': '2008', '10.0': '2010'}
  versions = []
  # For now, prefer versions before VS2010
  for version in ('9.0', '8.0', '10.0'):
    # Check if VS2010 and later is installed as specified by
    # http://msdn.microsoft.com/en-us/library/bb164659.aspx
    keys = [r'HKLM\SOFTWARE\Microsoft\DevDiv\VS\Servicing\%s' % version,
            r'HKLM\SOFTWARE\Wow6432Node\Microsoft\DevDiv\VS\Servicing\%s' % (
              version)]
    for index in range(len(keys)):
      if not _RegistryKeyExists(keys[index]):
        continue
      # Check for express
      if _RegistryKeyExists(keys[index] + '\\expbsln'):
        # Add this one
        versions.append(_CreateVersion(version_to_year[version] + 'e'))
      else:
        # Add this one
        versions.append(_CreateVersion(version_to_year[version]))

    # Old (pre-VS2010) method of searching for which VS version is installed
    keys = [r'HKLM\Software\Microsoft\VisualStudio\%s' % version,
            r'HKLM\Software\Wow6432Node\Microsoft\VisualStudio\%s' % version,
            r'HKLM\Software\Microsoft\VCExpress\%s' % version,
            r'HKLM\Software\Wow6432Node\Microsoft\VCExpress\%s' % version]
    for index in range(len(keys)):
      path = _RegistryGetValue(keys[index], 'InstallDir')
      if not path:
        continue
      # Check for full.
      if os.path.exists(os.path.join(path, 'devenv.exe')):
        # Add this one.
        versions.append(_CreateVersion(version_to_year[version]))
      # Check for express.
      elif os.path.exists(os.path.join(path, 'vcexpress.exe')):
        # Add this one.
        versions.append(_CreateVersion(version_to_year[version] + 'e'))
  return versions


def SelectVisualStudioVersion(version='auto'):
  """Select which version of Visual Studio projects to generate.

  Arguments:
    version: Hook to allow caller to force a particular version (vs auto).
  Returns:
    An object representing a visual studio project format version.
  """
  # In auto mode, check environment variable for override.
  if version == 'auto':
    version = os.environ.get('GYP_MSVS_VERSION', 'auto')
  # In auto mode, pick the most preferred version present.
  if version == 'auto':
    versions = _DetectVisualStudioVersions()
    if not versions:
      # Default to 2005.
      return _CreateVersion('2005')
    return versions[0]
  # Convert version string into a version object.
  return _CreateVersion(version)
