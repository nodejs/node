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

# In strict mode, function declarations may only appear as source elements.

# A template that performs the same strict-mode test in different
# scopes (global scope, function scope, and nested function scope).
def StrictTest(name, source, legacy):
  if legacy:
    extra_flags = [
      "--noharmony-scoping",
      "--noharmony-classes",
      "--noharmony-object-literals"]
  else:
    extra_flags = []
  Test(name, '"use strict";\n' + source, "strict_function",
       extra_flags)
  Test(name + '-infunc',
       'function foo() {\n "use strict";\n' + source +'\n}\n',
       "strict_function", 
       extra_flags)
  Test(name + '-infunc2',
       'function foo() {\n "use strict";\n  function bar() {\n' +
       source +'\n }\n}\n',
       "strict_function",
       extra_flags)

# Not testing with-scope, since with is not allowed in strict mode at all.

StrictTest("block", """
  { function foo() { } }
""", True)

StrictTest("try-w-catch", """
  try { function foo() { } } catch (e) { }
""", True)

StrictTest("try-w-finally", """
  try { function foo() { } } finally { }
""", True)

StrictTest("catch", """
  try { } catch (e) { function foo() { } }
""", True)

StrictTest("finally", """
  try { } finally { function foo() { } }
""", True)

StrictTest("for", """
  for (;;) { function foo() { } }
""", True)

StrictTest("while", """
  while (true) { function foo() { } }
""", True)

StrictTest("do", """
  do { function foo() { } } while (true);
""", True)

StrictTest("then", """
  if (true) { function foo() { } }
""", True)


StrictTest("then-w-else", """
  if (true) { function foo() { } } else { }
""", True)


StrictTest("else", """
  if (true) { } else { function foo() { } }
""", True)

StrictTest("switch-case", """
  switch (true) { case true: function foo() { } }
""", False)

StrictTest("labeled", """
  label: function foo() { }
""", False)



