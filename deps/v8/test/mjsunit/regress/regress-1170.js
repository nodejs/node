// Copyright 2011 the V8 project authors. All rights reserved.
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

// Flags: --es52_globals

var setter_value = 0;

this.__defineSetter__("a", function(v) { setter_value = v; });
eval("var a = 1");
assertEquals(1, setter_value);
assertFalse("value" in Object.getOwnPropertyDescriptor(this, "a"));

eval("with({}) { eval('var a = 2') }");
assertEquals(2, setter_value);
assertFalse("value" in Object.getOwnPropertyDescriptor(this, "a"));

// Function declarations are treated specially to match Safari. We do
// not call setters for them.
this.__defineSetter__("a", function(v) { assertUnreachable(); });
eval("function a() {}");
assertTrue("value" in Object.getOwnPropertyDescriptor(this, "a"));

this.__defineSetter__("b", function(v) { setter_value = v; });
try {
  eval("const b = 3");
} catch(e) {
  assertUnreachable();
}
assertEquals(3, setter_value);

try {
  eval("with({}) { eval('const b = 23') }");
} catch(e) {
  assertInstanceof(e, TypeError);
}

this.__defineSetter__("c", function(v) { throw 42; });
try {
  eval("var c = 1");
  assertUnreachable();
} catch(e) {
  assertEquals(42, e);
  assertFalse("value" in Object.getOwnPropertyDescriptor(this, "c"));
}




__proto__.__defineSetter__("aa", function(v) { assertUnreachable(); });
eval("var aa = 1");
assertTrue(this.hasOwnProperty("aa"));

__proto__.__defineSetter__("bb", function(v) { assertUnreachable(); });
eval("with({}) { eval('var bb = 2') }");
assertTrue(this.hasOwnProperty("bb"));

// Function declarations are treated specially to match Safari. We do
// not call setters for them.
__proto__.__defineSetter__("cc", function(v) { assertUnreachable(); });
eval("function cc() {}");
assertTrue(this.hasOwnProperty("cc"));

__proto__.__defineSetter__("dd", function(v) { assertUnreachable(); });
try {
  eval("const dd = 23");
} catch(e) {
  assertUnreachable();
}

try {
  eval("with({}) { eval('const dd = 23') }");
} catch(e) {
  assertInstanceof(e, TypeError);
}
