#!/usr/bin/env python
# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Utility functions to perform Xcode-style build steps.

These functions are executed via gyp-mac-tool when using the Makefile generator.
"""

import fcntl
import json
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
      # TODO(thakis): This copies file attributes like mtime, while the
      # single-file branch below doesn't. This should probably be changed to
      # be consistent with the single-file branch.
      if os.path.exists(dest):
        shutil.rmtree(dest)
      shutil.copytree(source, dest)
    elif extension == '.xib':
      return self._CopyXIBFile(source, dest)
    elif extension == '.storyboard':
      return self._CopyXIBFile(source, dest)
    elif extension == '.strings':
      self._CopyStringsFile(source, dest)
    else:
      shutil.copy(source, dest)

  def _CopyXIBFile(self, source, dest):
    """Compiles a XIB file with ibtool into a binary plist in the bundle."""

    # ibtool sometimes crashes with relative paths. See crbug.com/314728.
    base = os.path.dirname(os.path.realpath(__file__))
    if os.path.relpath(source):
      source = os.path.join(base, source)
    if os.path.relpath(dest):
      dest = os.path.join(base, dest)

    args = ['xcrun', 'ibtool', '--errors', '--warnings', '--notices',
        '--output-format', 'human-readable-text', '--compile', dest, source]
    ibtool_section_re = re.compile(r'/\*.*\*/')
    ibtool_re = re.compile(r'.*note:.*is clipping its content')
    ibtoolout = subprocess.Popen(args, stdout=subprocess.PIPE)
    current_section_header = None
    for line in ibtoolout.stdout:
      if ibtool_section_re.match(line):
        current_section_header = line
      elif not ibtool_re.match(line):
        if current_section_header:
          sys.stdout.write(current_section_header)
          current_section_header = None
        sys.stdout.write(line)
    return ibtoolout.returncode

  def _CopyStringsFile(self, source, dest):
    """Copies a .strings file using iconv to reconvert the input into UTF-16."""
    input_code = self._DetectInputEncoding(source) or "UTF-8"

    # Xcode's CpyCopyStringsFile / builtin-copyStrings seems to call
    # CFPropertyListCreateFromXMLData() behind the scenes; at least it prints
    #     CFPropertyListCreateFromXMLData(): Old-style plist parser: missing
    #     semicolon in dictionary.
    # on invalid files. Do the same kind of validation.
    import CoreFoundation
    s = open(source, 'rb').read()
    d = CoreFoundation.CFDataCreate(None, s, len(s))
    _, error = CoreFoundation.CFPropertyListCreateFromXMLData(None, d, 0, None)
    if error:
      return

    fp = open(dest, 'wb')
    fp.write(s.decode(input_code).encode('UTF-16'))
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
      return "UTF-16"
    elif header.startswith("\xFF\xFE"):
      return "UTF-16"
    elif header.startswith("\xEF\xBB\xBF"):
      return "UTF-8"
    else:
      return None

  def ExecCopyInfoPlist(self, source, dest, *keys):
    """Copies the |source| Info.plist to the destination directory |dest|."""
    # Read the source Info.plist into memory.
    fd = open(source, 'r')
    lines = fd.read()
    fd.close()

    # Insert synthesized key/value pairs (e.g. BuildMachineOSBuild).
    plist = plistlib.readPlistFromString(lines)
    if keys:
      plist = dict(plist.items() + json.loads(keys[0]).items())
    lines = plistlib.writePlistToString(plist)

    # Go through all the environment variables and replace them as variables in
    # the file.
    IDENT_RE = re.compile('[/\s]')
    for key in os.environ:
      if key.startswith('_'):
        continue
      evar = '${%s}' % key
      evalue = os.environ[key]
      lines = string.replace(lines, evar, evalue)

      # Xcode supports various suffices on environment variables, which are
      # all undocumented. :rfc1034identifier is used in the standard project
      # template these days, and :identifier was used earlier. They are used to
      # convert non-url characters into things that look like valid urls --
      # except that the replacement character for :identifier, '_' isn't valid
      # in a URL either -- oops, hence :rfc1034identifier was born.
      evar = '${%s:identifier}' % key
      evalue = IDENT_RE.sub('_', os.environ[key])
      lines = string.replace(lines, evar, evalue)

      evar = '${%s:rfc1034identifier}' % key
      evalue = IDENT_RE.sub('-', os.environ[key])
      lines = string.replace(lines, evar, evalue)

    # Remove any keys with values that haven't been replaced.
    lines = lines.split('\n')
    for i in range(len(lines)):
      if lines[i].strip().startswith("<string>${"):
        lines[i] = None
        lines[i - 1] = None
    lines = '\n'.join(filter(lambda x: x is not None, lines))

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
    signature_code = plist.get('CFBundleSignature', '????')
    if len(signature_code) != 4:  # Wrong length resets everything, too.
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
    """Calls libtool and filters out '/path/to/libtool: file: foo.o has no
    symbols'."""
    libtool_re = re.compile(r'^.*libtool: file: .* has no symbols$')
    libtoolout = subprocess.Popen(cmd_list, stderr=subprocess.PIPE)
    _, err = libtoolout.communicate()
    for line in err.splitlines():
      if not libtool_re.match(line):
        print >>sys.stderr, line
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
