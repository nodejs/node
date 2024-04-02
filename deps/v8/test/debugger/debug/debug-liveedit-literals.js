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

// Flags: --allow-natives-syntax

Debug = debug.Debug

function Test(old_expression, new_expression) {
    // Generate several instances of function to test that we correctly fix
    // all functions in memory.
    var function_instance_number = 11;
    eval("var t1 =1;\n" +
         "ChooseAnimalArray = [];\n" +
         "for (var i = 0; i < function_instance_number; i++) {\n" +
         "    ChooseAnimalArray.push(\n" +
         "        function ChooseAnimal() {\n" +
         "            return " + old_expression + ";\n" +
         "        });\n" +
         "}\n" +
         "var t2 =1;\n");

    for (var i = 0; i < ChooseAnimalArray.length; i++) {
        assertEquals("Cat", ChooseAnimalArray[i]());
    }

    var new_source =
        Debug.scriptSource(ChooseAnimalArray[0]).replace(old_expression, new_expression);
    %LiveEditPatchScript(ChooseAnimalArray[0], new_source);

    for (var i = 0; i < ChooseAnimalArray.length; i++) {
        assertEquals("Capybara", ChooseAnimalArray[i]());
    }
}

// Check that old literal boilerplate was reset.
Test("['Cat'][0]", "['Capybara'][0]");
Test("['Cat'][0]", "{a:'Capybara'}.a");

// No literals -> 1 literal.
Test("'Cat'", "['Capybara'][0]");

// No literals -> 2 literals.
Test("'Cat'", "['Capy'][0] + {a:'bara'}.a");

// 1 literal -> no literals.
Test("['Cat'][0]", "'Capybara'");

// 2 literals -> no literals.
Test("['Ca'][0] + {a:'t'}.a", "'Capybara'");

// No literals -> regexp.
Test("'Cat'", "(/.A.Y.A.A/i).exec('Capybara')[0]");

// Array literal -> regexp.
Test("['Cat'][0]", "(/.A.Y.A.A/i).exec('Capybara')[0]");

// Regexp -> object literal.
Test("(/.A./i).exec('Cat')[0]", "{c:'Capybara'}.c");

// No literals -> regexp.
Test("'Cat'", "(/.A.Y.A.A/i).exec('Capybara')[0]");

// Regexp -> no literals.
Test("(/.A./i).exec('Cat')[0]", "'Capybara'");
