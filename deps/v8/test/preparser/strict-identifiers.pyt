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

# Templatated tests with eval/arguments/future reserved words.

# ----------------------------------------------------------------------
# Constants and utility functions

reserved_words = [
  'class',
  'const', # Has other error message than other reserved words.
  'enum',
  'export',
  'extends',
  'import',
  'super'
  ]

strict_reserved_words = [
  'implements',
  'interface',
  'let',
  'package',
  'private',
  'protected',
  'public',
  'static',
  'yield'
  ]

assign_ops = {
  "=": "assign",
  "+=": "addeq",
  "-=": "subeq",
  "*=": "muleq",
  "/=": "diveq",
  "%=": "modeq",
  "&=": "andeq",
  "|=": "oreq",
  "^=": "xoreq",
  "<<=": "shleq",
  ">>=": "asreq",
  ">>>=": "lsreq"
  }


# A template that performs the same strict-mode test in different
# scopes (global scope, function scope, and nested function scope).
def StrictTemplate(name, source):
  def MakeTests(replacement, expectation):
    Template(name, '"use strict";\n' + source)(replacement, expectation)
    Template(name + '-infunc',
             'function foo() {\n "use strict";\n' + source +'\n}\n')(
              replacement, expectation)
    Template(name + '-infunc2',
             'function foo() {\n "use strict";\n  function bar() {\n' +
             source +'\n }\n}\n')(replacement, expectation)
  return MakeTests

# ----------------------------------------------------------------------
# Test templates

arg_name_own = Template("argument-name-own-$id", """
  function foo($id) {
    "use strict";
  }
""")

arg_name_nested = Template("argument-name-nested-$id", """
  function foo() {
    "use strict";
    function bar($id) { }
  }
""")

func_name_own = Template("function-name-own-$id", """
  function $id(foo) {
    "use strict";
  }
""")

func_name_nested = Template("function-name-nested-$id", """
  function foo() {
    "use strict";
    function $id(bar) { }
  }
""")

catch_var = StrictTemplate("catch-$id", """
    try { } catch ($id) { }
""")

declare_var = StrictTemplate("var-$id", """
  var $id = 42;
""")

assign_var = StrictTemplate("assign-$id-$opname", """
  var x = $id $op 42;
""")

prefix_var = StrictTemplate("prefix-$opname-$id", """
  var x = $op$id;
""")

postfix_var = StrictTemplate("postfix-$opname-$id", """
  var x = $id$op;
""")

read_var = StrictTemplate("read-reserved-$id", """
  var x = $id;
""")

setter_arg = StrictTemplate("setter-param-$id", """
  var x = {set foo($id) { }};
""")

label_normal = Template("label-normal-$id", """
  $id: '';
""")

label_strict = StrictTemplate("label-strict-$id", """
  $id: '';
""")

break_normal = Template("break-normal-$id", """
  for (;;) {
    break $id;
  }
""")

break_strict = StrictTemplate("break-strict-$id", """
  for (;;) {
    break $id;
  }
""")

continue_normal = Template("continue-normal-$id", """
  for (;;) {
    continue $id;
  }
""")

continue_strict = StrictTemplate("continue-strict-$id", """
  for (;;) {
    continue $id;
  }
""")

non_strict_use = Template("nonstrict-$id", """
  var $id = 42;
  $id++;
  $id--;
  ++$id;
  --$id;
  $id += 10;
  $id -= 10;
  try {} catch ($id) { }
  function $id($id) { }
  var x = {$id: 42};
  x = {get $id() {}, set $id(value) {}};
  function foo() { "use strict;" }
  var $id = 42;
  $id++;
  $id--;
  ++$id;
  --$id;
  $id += 10;
  $id -= 10;
  try {} catch ($id) { }
  function $id($id) { }
  x = {$id: 42};
  x = {get $id() {}, set $id(value) {}};
  $id: '';
""")

identifier_name_source = """
  var x = {$id: 42};
  x = {get $id() {}, set $id(value) {}};
  x.$id = 42;
  function foo() { "use strict;" }
  x = {$id: 42};
  x = {get $id() {}, set $id(value) {}};
  x.$id = 42;
"""

identifier_name = Template("identifier_name-$id", identifier_name_source)
identifier_name_strict = StrictTemplate("identifier_name_strict-$id",
                                        identifier_name_source)

# ----------------------------------------------------------------------
# Run tests

# eval and arguments have specific exceptions for different uses.
for id in ["eval", "arguments"]:
  arg_name_own({"id": id}, "strict_param_name")
  arg_name_nested({"id": id}, "strict_param_name")
  func_name_own({"id": id}, "strict_function_name")
  func_name_nested({"id": id}, "strict_function_name")
  setter_arg({"id": id}, "strict_param_name")
  for op in assign_ops.keys():
    assign_var({"id": id, "op":op, "opname": assign_ops[op]},
               "strict_lhs_assignment")
  catch_var({"id": id}, "strict_catch_variable")
  declare_var({"id": id}, "strict_var_name")
  prefix_var({"id": id, "op":"++", "opname":"inc"}, "strict_lhs_prefix")
  prefix_var({"id": id, "op":"--", "opname":"dec"}, "strict_lhs_prefix")
  postfix_var({"id": id, "op":"++", "opname":"inc"}, "strict_lhs_postfix")
  postfix_var({"id": id, "op":"--", "opname":"dec"}, "strict_lhs_postfix")
  label_normal({"id": id}, None)
  label_strict({"id": id}, None)
  break_normal({"id": id}, None)
  break_strict({"id": id}, None)
  continue_normal({"id": id}, None)
  continue_strict({"id": id}, None)
  non_strict_use({"id": id}, None)


# Reserved words just throw the same exception in all cases
# (with "const" being special, as usual).
for reserved_word in reserved_words + strict_reserved_words:
  if (reserved_word in strict_reserved_words):
    message = "strict_reserved_word"
    label_message = None
  elif (reserved_word == "const"):
    message = "unexpected_token"
    label_message = message
  else:
    message = "reserved_word"
    label_message = message
  arg_name_own({"id":reserved_word}, message)
  arg_name_nested({"id":reserved_word}, message)
  setter_arg({"id": reserved_word}, message)
  func_name_own({"id":reserved_word}, message)
  func_name_nested({"id":reserved_word}, message)
  for op in assign_ops.keys():
    assign_var({"id":reserved_word, "op":op, "opname": assign_ops[op]}, message)
  catch_var({"id":reserved_word}, message)
  declare_var({"id":reserved_word}, message)
  prefix_var({"id":reserved_word, "op":"++", "opname":"inc"}, message)
  prefix_var({"id":reserved_word, "op":"--", "opname":"dec"}, message)
  postfix_var({"id":reserved_word, "op":"++", "opname":"inc"}, message)
  postfix_var({"id":reserved_word, "op":"--", "opname":"dec"}, message)
  read_var({"id": reserved_word}, message)
  identifier_name({"id": reserved_word}, None);
  identifier_name_strict({"id": reserved_word}, None);
  label_normal({"id": reserved_word}, label_message)
  break_normal({"id": reserved_word}, label_message)
  continue_normal({"id": reserved_word}, label_message)
  if (reserved_word == "const"):
    # The error message for this case is different because
    # ParseLabelledStatementOrExpression will try to parse this as an expression
    # first, effectively disallowing the use in ParseVariableDeclarations, i.e.
    # the preparser never sees that 'const' was intended to be a label.
    label_strict({"id": reserved_word}, "strict_const")
  else:
    label_strict({"id": reserved_word}, message)
  break_strict({"id": reserved_word}, message)
  continue_strict({"id": reserved_word}, message)


# Future reserved words in strict mode behave like normal identifiers
# in a non strict context.
for reserved_word in strict_reserved_words:
  non_strict_use({"id": reserved_word}, None)
