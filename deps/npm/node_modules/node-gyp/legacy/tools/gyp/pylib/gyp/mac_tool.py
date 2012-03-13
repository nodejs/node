#!/usr/bin/env python
# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Utility functions to perform Xcode-style build steps.

These functions are executed via gyp-mac-tool when using the Makefile generator.
"""

import fcntl
import os
import plistlib
import re
import shutil
import string
import subprocess
import sys


def main(args):
  executor = MacTool()
  exit_code = executor.Dispatch(args)
  if exit_code is not None:
    sys.exit(exit_code)


class MacTool(object):
  """This class performs all the Mac tooling steps. The methods can either be
  executed directly, or dispatched from an argument list."""

  def Dispatch(self, args):
    """Dispatches a string command to a method."""
    if len(args) < 1:
      raise Exception("Not enough arguments")

    method = "Exec%s" % self._CommandifyName(args[0])
    return getattr(self, method)(*args[1:])

  def _CommandifyName(self, name_string):
    """Transforms a tool name like copy-info-plist to CopyInfoPlist"""
    return name_string.title().replace('-', '')

  def ExecCopyBundleResource(self, source, dest):
    """Copies a resource file to the bundle/Resources directory, performing any
    necessary compilation on each resource."""
    extension = os.path.splitext(source)[1].lower()
    if os.path.isdir(source):
      # Copy tree.
      if os.path.exists(dest):
        shutil.rmtree(dest)
      shutil.copytree(source, dest)
    elif extension == '.xib':
      self._CopyXIBFile(source, dest)
    elif extension == '.strings':
      self._CopyStringsFile(source, dest)
    # TODO: Given that files with arbitrary extensions can be copied to the
    # bundle, we will want to get rid of this whitelist eventually.
    elif extension in [
        '.icns', '.manifest', '.pak', '.pdf', '.png', '.sb', '.sh',
        '.ttf', '.sdef']:
      shutil.copyfile(source, dest)
    else:
      raise NotImplementedError(
          "Don't know how to copy bundle resources of type %s while copying "
          "%s to %s)" % (extension, source, dest))

  def _CopyXIBFile(self, source, dest):
    """Compiles a XIB file with ibtool into a binary plist in the bundle."""
    args = ['/Developer/usr/bin/ibtool', '--errors', '--warnings',
        '--notices', '--output-format', 'human-readable-text', '--compile',
        dest, source]
    subprocess.call(args)

  def _CopyStringsFile(self, source, dest):
    """Copies a .strings file using iconv to reconvert the input into UTF-16."""
    input_code = self._DetectInputEncoding(source) or "UTF-8"
    fp = open(dest, 'w')
    args = ['/usr/bin/iconv', '--from-code', input_code, '--to-code',
        'UTF-16', source]
    subprocess.call(args, stdout=fp)
    fp.close()

  def _DetectInputEncoding(self, file_name):
    """Reads the first few bytes from file_name and tries to guess the text
    encoding. Returns None as a guess if it can't detect it."""
    fp = open(file_name, 'rb')
    try:
      header = fp.read(3)
    except e:
      fp.close()
      return None
    fp.close()
    if header.startswith("\xFE\xFF"):
      return "UTF-16BE"
    elif header.startswith("\xFF\xFE"):
      return "UTF-16LE"
    elif header.startswith("\xEF\xBB\xBF"):
      return "UTF-8"
    else:
      return None

  def ExecCopyInfoPlist(self, source, dest):
    """Copies the |source| Info.plist to the destination directory |dest|."""
    # Read the source Info.plist into memory.
    fd = open(source, 'r')
    lines = fd.read()
    fd.close()

    # Go through all the environment variables and replace them as variables in
    # the file.
    for key in os.environ:
      if key.startswith('_'):
        continue
      evar = '${%s}' % key
      lines = string.replace(lines, evar, os.environ[key])

    # Write out the file with variables replaced.
    fd = open(dest, 'w')
    fd.write(lines)
    fd.close()

    # Now write out PkgInfo file now that the Info.plist file has been
    # "compiled".
    self._WritePkgInfo(dest)

  def _WritePkgInfo(self, info_plist):
    """This writes the PkgInfo file from the data stored in Info.plist."""
    plist = plistlib.readPlist(info_plist)
    if not plist:
      return

    # Only create PkgInfo for executable types.
    package_type = plist['CFBundlePackageType']
    if package_type != 'APPL':
      return

    # The format of PkgInfo is eight characters, representing the bundle type
    # and bundle signature, each four characters. If that is missing, four
    # '?' characters are used instead.
    signature_code = plist['CFBundleSignature']
    if len(signature_code) != 4:
      signature_code = '?' * 4

    dest = os.path.join(os.path.dirname(info_plist), 'PkgInfo')
    fp = open(dest, 'w')
    fp.write('%s%s' % (package_type, signature_code))
    fp.close()

  def ExecFlock(self, lockfile, *cmd_list):
    """Emulates the most basic behavior of Linux's flock(1)."""
    # Rely on exception handling to report errors.
    fd = os.open(lockfile, os.O_RDONLY|os.O_NOCTTY|os.O_CREAT, 0o666)
    fcntl.flock(fd, fcntl.LOCK_EX)
    return subprocess.call(cmd_list)

  def ExecFilterLibtool(self, *cmd_list):
    """Calls libtool and filters out 'libtool: file: foo.o has no symbols'."""
    libtool_re = re.compile(r'^libtool: file: .* has no symbols$')
    libtoolout = subprocess.Popen(cmd_list, stderr=subprocess.PIPE)
    for line in libtoolout.stderr:
      if not libtool_re.match(line):
        sys.stderr.write(line)
    return libtoolout.returncode

  def ExecPackageFramework(self, framework, version):
    """Takes a path to Something.framework and the Current version of that and
    sets up all the symlinks."""
    # Find the name of the binary based on the part before the ".framework".
    binary = os.path.basename(framework).split('.')[0]

    CURRENT = 'Current'
    RESOURCES = 'Resources'
    VERSIONS = 'Versions'

    if not os.path.exists(os.path.join(framework, VERSIONS, version, binary)):
      # Binary-less frameworks don't seem to contain symlinks (see e.g.
      # chromium's out/Debug/org.chromium.Chromium.manifest/ bundle).
      return

    # Move into the framework directory to set the symlinks correctly.
    pwd = os.getcwd()
    os.chdir(framework)

    # Set up the Current version.
    self._Relink(version, os.path.join(VERSIONS, CURRENT))

    # Set up the root symlinks.
    self._Relink(os.path.join(VERSIONS, CURRENT, binary), binary)
    self._Relink(os.path.join(VERSIONS, CURRENT, RESOURCES), RESOURCES)

    # Back to where we were before!
    os.chdir(pwd)

  def _Relink(self, dest, link):
    """Creates a symlink to |dest| named |link|. If |link| already exists,
    it is overwritten."""
    if os.path.lexists(link):
      os.remove(link)
    os.symlink(dest, link)


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
