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

// Flags: --expose-debug-as debug
// Get the Debug object exposed from the debug context global object.

// In this test case we edit a script so that techincally function text
// hasen't been changed. However actually function became one level more nested
// and must be recompiled because it uses variable from outer scope.


Debug = debug.Debug

var function_z_text =
"  function Z() {\n"
+ "    return 2 + p;\n"
+ "  }\n";

eval(
"function Factory(p) {\n"
+ "return (\n"
+ function_z_text
+ ");\n"
+ "}\n"
);

var z6 = Factory(6);
assertEquals(8, z6());

var script = Debug.findScript(Factory);

var new_source = script.source.replace(function_z_text, "function Intermediate() {\nreturn (\n" + function_z_text + ")\n;\n}\n");
print("new source: " + new_source);

var change_log = new Array();
Debug.LiveEdit.SetScriptSource(script, new_source, change_log);
print("Change log: " + JSON.stringify(change_log) + "\n");

assertEquals(8, z6());

var z100 = Factory(100)();

assertEquals(102, z100());


