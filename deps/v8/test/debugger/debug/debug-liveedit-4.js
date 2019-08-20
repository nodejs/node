// Copyright 2010 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// Flags: --noalways-opt

// In this test case we edit a script so that techincally function text
// hasen't been changed. However actually function became one level more nested
// and must be recompiled because it uses variable from outer scope.

// Flags: --allow-natives-syntax

Debug = debug.Debug

eval(
"function TestFunction() {\n"
+ "  var a = 'a';\n"
+ "  var b = 'b';\n"
+ "  var c = 'c';\n"
+ "  function A() {\n"
+ "    return 2013;\n"
+ "  }\n"
+ "  function B() {\n"
+ "    return String([a, c]);\n"
+ "  }\n"
+ "  return B();\n"
+ "}\n"
);

var res = TestFunction();
print(res);
assertEquals('a,c', res);

var new_source = Debug.scriptSource(TestFunction).replace('2013', 'b');
print("new source: " + new_source);

%LiveEditPatchScript(TestFunction, new_source);

var res = TestFunction();
print(res);
// This might be 'a,b' without a bug fixed.
assertEquals('a,c', res);
