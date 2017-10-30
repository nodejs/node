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


Debug = debug.Debug

eval("var something1 = 25; \n"
     + "var something2 = 2010; \n"
     + "// Array(); \n"
     + "function ChooseAnimal() {\n"
     + "  return 'Cat';\n"
     + "} \n"
     + "function ChooseFurniture() {\n"
     + "  return 'Table';\n"
     + "} \n"
     + "function ChooseNumber() { return 17; } \n"
     + "ChooseAnimal.Factory = function Factory() {\n"
     + "  return function FactoryImpl(name) {\n"
     + "    return 'Help ' + name;\n"
     + "  }\n"
     + "}\n");

assertEquals("Cat", ChooseAnimal());
assertEquals(25, something1);

var script = Debug.findScript(ChooseAnimal);

var new_source = script.source.replace("Cat", "Cap' + 'yb' + 'ara");
var new_source = new_source.replace("25", "26");
var new_source = new_source.replace("Help", "Hello");
var new_source = new_source.replace("17", "18");
// The call to array causes a change in the number of type feedback slots for
// the script.
//
// TODO(mvstanton): For now, the inclusion of the Array() call at the top level
// of the script causes us to visit a corner case, but I'd like to validate
// correctness more explicitly.
var new_source = new_source.replace("// Array", "Array");
print("new source: " + new_source);

var change_log = new Array();
var result = Debug.LiveEdit.SetScriptSource(script, new_source, false, change_log);
print("Result: " + JSON.stringify(result) + "\n");
print("Change log: " + JSON.stringify(change_log) + "\n");

assertEquals("Capybara", ChooseAnimal());
// Global variable do not get changed (without restarting script).
assertEquals(25, something1);
// We should support changes in oneliners.
assertEquals(18, ChooseNumber());
assertEquals("Hello Peter", ChooseAnimal.Factory()("Peter"));
