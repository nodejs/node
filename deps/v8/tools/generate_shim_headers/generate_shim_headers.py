#!/usr/bin/env python
#
# Copyright 2013 the V8 project authors. All rights reserved.
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

"""
Generates shim headers that mirror the directory structure of bundled headers,
but just forward to the system ones.

This allows seamless compilation against system headers with no changes
to our source code.
"""


import optparse
import os.path
import sys


def GeneratorMain(argv):
  parser = optparse.OptionParser()
  parser.add_option('--headers-root', action='append')
  parser.add_option('--define', action='append')
  parser.add_option('--output-directory')
  parser.add_option('--prefix', default='')
  parser.add_option('--use-include-next', action='store_true')
  parser.add_option('--outputs', action='store_true')
  parser.add_option('--generate', action='store_true')

  options, args = parser.parse_args(argv)

  if not options.headers_root:
    parser.error('Missing --headers-root parameter.')
  if not options.output_directory:
    parser.error('Missing --output-directory parameter.')
  if not args:
    parser.error('Missing arguments - header file names.')

  source_tree_root = os.path.abspath(
    os.path.join(os.path.dirname(__file__), '..', '..'))

  for root in options.headers_root:
    target_directory = os.path.join(
      options.output_directory,
      os.path.relpath(root, source_tree_root))
    if options.generate and not os.path.exists(target_directory):
      os.makedirs(target_directory)

    for header_spec in args:
      if ';' in header_spec:
        (header_filename,
         include_before,
         include_after) = header_spec.split(';', 2)
      else:
        header_filename = header_spec
        include_before = ''
        include_after = ''
      if options.outputs:
        yield os.path.join(target_directory, header_filename)
      if options.generate:
        with open(os.path.join(target_directory, header_filename), 'w') as f:
          if options.define:
            for define in options.define:
              key, value = define.split('=', 1)
              # This non-standard push_macro extension is supported
              # by compilers we support (GCC, clang).
              f.write('#pragma push_macro("%s")\n' % key)
              f.write('#undef %s\n' % key)
              f.write('#define %s %s\n' % (key, value))

          if include_before:
            for header in include_before.split(':'):
              f.write('#include %s\n' % header)

          include_target = options.prefix + header_filename
          if options.use_include_next:
            f.write('#include_next <%s>\n' % include_target)
          else:
            f.write('#include <%s>\n' % include_target)

          if include_after:
            for header in include_after.split(':'):
              f.write('#include %s\n' % header)

          if options.define:
            for define in options.define:
              key, value = define.split('=', 1)
              # This non-standard pop_macro extension is supported
              # by compilers we support (GCC, clang).
              f.write('#pragma pop_macro("%s")\n' % key)


def DoMain(argv):
  return '\n'.join(GeneratorMain(argv))


if __name__ == '__main__':
  DoMain(sys.argv[1:])
