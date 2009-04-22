// Copyright 2009 the V8 project authors. All rights reserved.
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

// Test the handling of initialization of deleted const variables.
// This only makes sense in local scopes since the declaration and
// initialization of consts in the global scope happen at the same
// time.

function testIntroduceGlobal() {
  var source =
      // Deleting 'x' removes the local const property.
      "delete x;" +
      // Initialization turns into assignment to global 'x'.
      "const x = 3; assertEquals(3, x);" +
      // No constness of the global 'x'.
      "x = 4; assertEquals(4, x);";
  eval(source);
}

testIntroduceGlobal();
assertEquals(4, x);

function testAssignExistingGlobal() {
  var source =
      // Delete 'x' to remove the local const property.
      "delete x;" +
      // Initialization turns into assignment to global 'x'.
      "const x = 5; assertEquals(5, x);" +
      // No constness of the global 'x'.
      "x = 6; assertEquals(6, x);";
  eval(source);
}

testAssignExistingGlobal();
assertEquals(6, x);

function testAssignmentArgument(x) {
  function local() {
    var source = "delete x; const x = 7; assertEquals(7, x)";
    eval(source);
  }
  local();
  assertEquals(7, x);
}

testAssignmentArgument();
assertEquals(6, x);

__defineSetter__('x', function() { throw 42; });
function testAssignGlobalThrows() {
  // Initialization turns into assignment to global 'x' which
  // throws an exception.
  var source = "delete x; const x = 8";
  eval(source);
}

assertThrows("testAssignGlobalThrows()");

function testInitFastCaseExtension() {
  var source = "const x = 9; assertEquals(9, x); x = 10; assertEquals(9, x)";
  eval(source);
}

testInitFastCaseExtension();

function testInitSlowCaseExtension() {
  var source = "";
  // Introduce 100 properties on the context extension object to force
  // it in slow case.
  for (var i = 0; i < 100; i++) source += ("var a" + i + " = i;");
  source += "const x = 10; assertEquals(10, x); x = 11; assertEquals(10, x)";
  eval(source);
}

testInitSlowCaseExtension();

function testAssignSurroundingContextSlot() {
  var x = 12;
  function local() {
    var source = "delete x; const x = 13; assertEquals(13, x)";
    eval(source);
  }
  local();
  assertEquals(13, x);
}

testAssignSurroundingContextSlot();
