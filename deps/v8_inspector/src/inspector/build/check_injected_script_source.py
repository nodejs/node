#!/usr/bin/env python
# Copyright (c) 2014 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
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
#
# Copied from blink:
# WebKit/Source/devtools/scripts/check_injected_script_source.py
#

import re
import sys
import os


def validate_injected_script(fileName):
    f = open(fileName, "r")
    lines = f.readlines()
    f.close()

    proto_functions = "|".join([
        # Array.prototype.*
        "concat", "every", "filter", "forEach", "indexOf", "join", "lastIndexOf", "map", "pop",
        "push", "reduce", "reduceRight", "reverse", "shift", "slice", "some", "sort", "splice", "toLocaleString", "toString", "unshift",
        # Function.prototype.*
        "apply", "bind", "call", "isGenerator", "toSource",
        # Object.prototype.*
        "toString",
    ])

    global_functions = "|".join([
        "eval", "uneval", "isFinite", "isNaN", "parseFloat", "parseInt", "decodeURI", "decodeURIComponent",
        "encodeURI", "encodeURIComponent", "escape", "unescape", "Map", "Set"
    ])

    # Black list:
    # - instanceof, since e.g. "obj instanceof Error" may throw if Error is overridden and is not a function
    # - Object.prototype.toString()
    # - Array.prototype.*
    # - Function.prototype.*
    # - Math.*
    # - Global functions
    black_list_call_regex = re.compile(r"\sinstanceof\s+\w*|\bMath\.\w+\(|(?<!InjectedScriptHost)\.(" + proto_functions + r")\(|[^\.]\b(" + global_functions + r")\(")

    errors_found = False
    for i, line in enumerate(lines):
        if line.find("suppressBlacklist") != -1:
            continue
        for match in re.finditer(black_list_call_regex, line):
            errors_found = True
            print "ERROR: Black listed expression in %s at line %02d column %02d: %s" % (os.path.basename(fileName), i + 1, match.start(), match.group(0))

    if not errors_found:
        print "OK"


def main(argv):
    if len(argv) < 2:
        print('ERROR: Usage: %s path/to/injected-script-source.js' % argv[0])
        return 1

    validate_injected_script(argv[1])

if __name__ == '__main__':
    sys.exit(main(sys.argv))
