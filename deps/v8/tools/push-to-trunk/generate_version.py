#!/usr/bin/env python
# Copyright 2014 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Script to set v8's version file to the version given by the latest tag.
"""


import os
import re
import subprocess
import sys


CWD = os.path.abspath(
    os.path.dirname(os.path.dirname(os.path.dirname(__file__))))
VERSION_CC = os.path.join(CWD, "src", "version.cc")

def main():
  tag = subprocess.check_output(
      "git describe --tags",
      shell=True,
      cwd=CWD,
  ).strip()
  assert tag

  # Check for commits not exactly matching a tag. Those are candidate builds
  # for the next version. The output has the form
  # <tag name>-<n commits>-<hash>.
  if "-" in tag:
    version = tag.split("-")[0]
    candidate = "1"
  else:
    version = tag
    candidate = "0"
  version_levels = version.split(".")

  # Set default patch level if none is given.
  if len(version_levels) == 3:
    version_levels.append("0")
  assert len(version_levels) == 4

  major, minor, build, patch = version_levels

  # Increment build level for candidate builds.
  if candidate == "1":
    build = str(int(build) + 1)
    patch = "0"

  # Modify version.cc with the new values.
  with open(VERSION_CC, "r") as f:
    text = f.read()
  output = []
  for line in text.split("\n"):
    for definition, substitute in (
        ("MAJOR_VERSION", major),
        ("MINOR_VERSION", minor),
        ("BUILD_NUMBER", build),
        ("PATCH_LEVEL", patch),
        ("IS_CANDIDATE_VERSION", candidate)):
      if line.startswith("#define %s" % definition):
        line =  re.sub("\d+$", substitute, line)
    output.append(line)
  with open(VERSION_CC, "w") as f:
    f.write("\n".join(output))

  # Log what was done.
  candidate_txt = " (candidate)" if candidate == "1" else ""
  patch_txt = ".%s" % patch if patch != "0" else ""
  version_txt = ("%s.%s.%s%s%s" %
                 (major, minor, build, patch_txt, candidate_txt))
  print "Modified version.cc. Set V8 version to %s" %  version_txt
  return 0

if __name__ == "__main__":
  sys.exit(main())
