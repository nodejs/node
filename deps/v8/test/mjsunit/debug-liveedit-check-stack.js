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
    "function ChooseAnimal(callback) {\n " +
    "  callback();\n" +
    "  return 'Cat';\n" +
    "}\n"
);

function Noop() {}
var res = ChooseAnimal(Noop);

assertEquals("Cat", res);

var script = Debug.findScript(ChooseAnimal);

var orig_animal = "'Cat'";
var patch_pos = script.source.indexOf(orig_animal);
var new_animal_patch = "'Capybara'";

var got_exception = false;
var successfully_changed = false;

function Changer() {
  // Never try the same patch again.
  assertEquals(false, successfully_changed);
  var change_log = new Array();
  try {
    Debug.LiveEditChangeScript(script, patch_pos, orig_animal.length, new_animal_patch, change_log);
    successfully_changed = true;
  } catch (e) {
    if (e instanceof Debug.LiveEditChangeScript.Failure) {
      got_exception = true;
      print(e);
    } else {
      throw e;
    }
  }
  print("Change log: " + JSON.stringify(change_log) + "\n");
}

var new_res = ChooseAnimal(Changer);
// Function must be not pached.
assertEquals("Cat", new_res);

assertEquals(true, got_exception);

// This time it should succeed.
Changer();

new_res = ChooseAnimal(Noop);
// Function must be not pached.
assertEquals("Capybara", new_res);

