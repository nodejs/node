# Copyright 2016 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Represent a file as a C++ constant string.

Usage:
python xxd.py VAR SOURCE DEST
"""


import sys
import rjsmin


def main():
    variable_name, input_filename, output_filename = sys.argv[1:]
    with open(input_filename) as input_file:
        input_text = input_file.read()
    input_text = rjsmin.jsmin(input_text)
    hex_values = ['0x{0:02x}'.format(ord(char)) for char in input_text]
    const_declaration = 'const char %s[] = {\n%s\n};\n' % (
        variable_name, ', '.join(hex_values))
    with open(output_filename, 'w') as output_file:
        output_file.write(const_declaration)

if __name__ == '__main__':
    sys.exit(main())
