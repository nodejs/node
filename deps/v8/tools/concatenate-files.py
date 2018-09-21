#!/usr/bin/env python
#
# Copyright 2014 the V8 project authors. All rights reserved.
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
#       copyright notice, this list of conditions and the following
#       disclaimer in the documentation and/or other materials provided
#       with the distribution.
#     * Neither the name of Google Inc. nor the names of its
#       contributors may be used to endorse or promote products derived
#       from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# This utility concatenates several files into one. On Unix-like systems
# it is equivalent to:
#   cat file1 file2 file3 ...files... > target
#
# The reason for writing a separate utility is that 'cat' is not available
# on all supported build platforms, but Python is, and hence this provides
# us with an easy and uniform way of doing this on all platforms.

import optparse


def Concatenate(filenames):
  """Concatenate files.

  Args:
    files: Array of file names.
           The last name is the target; all earlier ones are sources.

  Returns:
    True, if the operation was successful.
  """
  if len(filenames) < 2:
    print "An error occurred generating %s:\nNothing to do." % filenames[-1]
    return False

  try:
    with open(filenames[-1], "wb") as target:
      for filename in filenames[:-1]:
        with open(filename, "rb") as current:
          target.write(current.read())
    return True
  except IOError as e:
    print "An error occurred when writing %s:\n%s" % (filenames[-1], e)
    return False


def main():
  parser = optparse.OptionParser()
  parser.set_usage("""Concatenate several files into one.
      Equivalent to: cat file1 ... > target.""")
  (options, args) = parser.parse_args()
  exit(0 if Concatenate(args) else 1)


if __name__ == "__main__":
  main()
