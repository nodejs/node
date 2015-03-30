# Copyright (c) 2014 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
TestWin.py:  a collection of helpers for testing on Windows.
"""

import errno
import os
import re
import sys
import subprocess

class Registry(object):
  def _QueryBase(self, sysdir, key, value):
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
    # Get the stdout from reg.exe, reading to the end so p.returncode is valid
    # Note that the error text may be in [1] in some cases
    text = p.communicate()[0]
    # Check return code from reg.exe; officially 0==success and 1==error
    if p.returncode:
      return None
    return text

  def Query(self, key, value=None):
    r"""Use reg.exe to read a particular key through _QueryBase.

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
      text = self._QueryBase('Sysnative', key, value)
    except OSError, e:
      if e.errno == errno.ENOENT:
        text = self._QueryBase('System32', key, value)
      else:
        raise
    return text

  def GetValue(self, key, value):
    """Use reg.exe to obtain the value of a registry key.

    Args:
      key: The registry key.
      value: The particular registry value to read.
    Return:
      contents of the registry key's value, or None on failure.
    """
    text = self.Query(key, value)
    if not text:
      return None
    # Extract value.
    match = re.search(r'REG_\w+\s+([^\r]+)\r\n', text)
    if not match:
      return None
    return match.group(1)

  def KeyExists(self, key):
    """Use reg.exe to see if a key exists.

    Args:
      key: The registry key to check.
    Return:
      True if the key exists
    """
    if not self.Query(key):
      return False
    return True
