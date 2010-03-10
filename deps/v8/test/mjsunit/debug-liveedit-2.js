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


Debug = debug.Debug


eval(
    "function ChooseAnimal(p) {\n " +
    "  if (p == 7) {\n" + // Use p
    "    return;\n" +
    "  }\n" +
    "  return function Chooser() {\n" +
    "    return 'Cat';\n" +
    "  };\n" +
    "}\n"
);

var old_closure = ChooseAnimal(19);

assertEquals("Cat", old_closure());

var script = Debug.findScript(ChooseAnimal);

var orig_animal = "'Cat'";
var patch_pos = script.source.indexOf(orig_animal);
var new_animal_patch = "'Capybara' + p";

// We patch innermost function "Chooser".
// However, this does not actually patch existing "Chooser" instances,
// because old value of parameter "p" was not saved.
// Instead it patches ChooseAnimal.
var change_log = new Array();
Debug.LiveEditChangeScript(script, patch_pos, orig_animal.length, new_animal_patch, change_log);
print("Change log: " + JSON.stringify(change_log) + "\n");

var new_closure = ChooseAnimal(19);
// New instance of closure is patched.
assertEquals("Capybara19", new_closure());

// Old instance of closure is not patched.
assertEquals("Cat", old_closure());

