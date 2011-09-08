# Copyright 2011 the V8 project authors. All rights reserved.
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

# Templatated tests with duplicate parameter names.

# ----------------------------------------------------------------------
# Constants and utility functions

# A template that performs the same strict-mode test in different
# scopes (global scope, function scope, and nested function scope),
# and in non-strict mode too.
def DuplicateParameterTest(name, source):
  expectation = "strict_param_dupe"
  non_selfstrict = {"selfstrict":"", "id":"selfnormal"}

  Template(name, '"use strict";\n' + source)(non_selfstrict, expectation)
  Template(name + '-infunc',
           'function foo() {\n "use strict";\n' + source +'\n}\n')(
               non_selfstrict, expectation)
  Template(name + '-infunc2',
           'function foo() {\n "use strict";\n  function bar() {\n' +
           source +'\n }\n}\n')(non_selfstrict, expectation)

  selfstrict = {"selfstrict": "\"use strict\";", "id": "selfstrict"}
  nestedstrict = {"selfstrict": "function bar(){\"use strict\";}",
                  "id": "nestedstrict"}
  selfstrictnestedclean = {"selfstrict": """
      "use strict";
      function bar(){}
    """, "id": "selfstrictnestedclean"}
  selftest = Template(name + '-$id', source)
  selftest(selfstrict, expectation)
  selftest(selfstrictnestedclean, expectation)
  selftest(nestedstrict, None)
  selftest(non_selfstrict, None)


# ----------------------------------------------------------------------
# Test templates

DuplicateParameterTest("dups", """
  function foo(a, a) { $selfstrict }
""");

DuplicateParameterTest("dups-apart", """
  function foo(a, b, c, d, e, f, g, h, i, j, k, l, m, n, a) { $selfstrict }
""");

DuplicateParameterTest("dups-escaped", """
  function foo(\u0061, b, c, d, e, f, g, h, i, j, k, l, m, n, a) { $selfstrict }
""");

DuplicateParameterTest("triples", """
  function foo(a, b, c, d, e, f, g, h, a, i, j, k, l, m, n, a) { $selfstrict }
""");

DuplicateParameterTest("escapes", """
  function foo(a, \u0061) { $selfstrict }
""");

DuplicateParameterTest("long-names", """
  function foo(arglebargleglopglyfarglebargleglopglyfarglebargleglopglyfa,
               arglebargleglopglyfarglebargleglopglyfarglebargleglopglyfa) {
    $selfstrict
  }
""");
