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


// Scenario: a function is being changed, which causes enclosing function to
// have its positions patched; position changing requires new instance of Code
// object to be introduced; the function happens to be on stack at this moment;
// later it will resume over new instance of Code.
// Before the change 2 rinfo are 22 characters away from each other. After the
// change they are 114 characters away from each other. New instance of Code is
// required when those numbers cross the border value of 64 (in any direction).

// Flags: --allow-natives-syntax
Debug = debug.Debug

eval(
    "function BeingReplaced(changer, opt_x, opt_y) {\n" +
    "  changer();\n" +
    "  var res = new Object();\n" +
    "  if (opt_x) { res.y = opt_y; }\n" +
    "  res.a = (function() {})();\n" +
    "  return res.a;\n" +
    "}"
);

function Changer() {
  // Line long enough to change rinfo encoding.
  var new_source =
    Debug.scriptSource(BeingReplaced).replace("{}", "{return 'Capybara';" +
    "                                                                          " +
    "}");
  %LiveEditPatchScript(BeingReplaced, new_source);
}

function NoOp() {
}

function CallM(changer) {
  // We expect call IC here after several function runs.
  return BeingReplaced(changer);
}

// This several iterations should cause call IC for BeingReplaced call. This IC
// will keep reference to code object of BeingReplaced function. This reference
// should also be patched. Unfortunately, this is a manually checked fact (from
// debugger or debug print) and doesn't work as an automatic test.
CallM(NoOp);
CallM(NoOp);
CallM(NoOp);

var res = CallM(Changer);
assertEquals("Capybara", res);
