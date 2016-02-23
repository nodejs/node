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

// Make sure that eval can introduce a local variable called __proto__.
// See http://code.google.com/p/v8/issues/detail?id=186

var setterCalled = false;

var o = {};
o.__defineSetter__("x", function() { setterCalled = true; });

function runTest(test) {
  setterCalled = false;
  test();
}

function testLocal() {
  // Add property called __proto__ to the extension object.
  eval("var __proto__ = o");
  // Check that the extension object's prototype did not change.
  eval("var x = 27");
  assertFalse(setterCalled, "prototype of extension object changed");
  assertEquals(o, eval("__proto__"));
}

function testConstLocal() {
  // Add const property called __proto__ to the extension object.
  eval("const __proto__ = o");
  // Check that the extension object's prototype did not change.
  eval("var x = 27");
  assertFalse(setterCalled, "prototype of extension object changed");
  assertEquals(o, eval("__proto__"));
}

function testGlobal() {
  // Assign to the global __proto__ property.
  eval("__proto__ = o");
  // Check that the prototype of the global object changed.
  eval("x = 27");
  assertTrue(setterCalled, "prototype of global object did not change");
  setterCalled = false;
  assertEquals(o, eval("__proto__"));
}

runTest(testLocal);
runTest(testConstLocal);
runTest(testGlobal);
