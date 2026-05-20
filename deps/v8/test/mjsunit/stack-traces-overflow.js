// Copyright 2012 the V8 project authors. All rights reserved.
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

// Flags: --stack-size=100

function overflow() {
  var a, b, c, d, e;  // Allocates some locals on the function's stack frame.
  overflow();
}
function rec1(a) { rec1(a+1); }
function rec2(a) { rec3(a+1); }
function rec3(a) { rec2(a+1); }

// Test stack trace has correct function location at top of the stack.
try {
  overflow();
} catch (e) {
  var first_frame = e.stack.split("\n")[1]
  // The overflow can happen when pushing the arguments (in interpreter) or when
  // the new function execution is starting. So the stack trace could either
  // point to start of the function (stack-traces-overflow.js30:18) or to the
  // location of call (stack-traces-overflow.js32:3).
  assertTrue((first_frame.indexOf("stack-traces-overflow.js:30:18") > 0) ||
             (first_frame.indexOf("stack-traces-overflow.js:32:3") > 0) );
}

// Test stack trace getter and setter.
try {
  rec1(0);
} catch (e) {
  assertTrue(e.stack.indexOf("rec1") > 0);
  e.stack = "123";
  assertEquals("123", e.stack);
}

// Test setter w/o calling the getter.
try {
  rec2(0);
} catch (e) {
  assertTrue(e.stack.indexOf("rec2") > 0);
  assertTrue(e.stack.indexOf("rec3") > 0);
  e.stack = "123";
  assertEquals("123", e.stack);
}

// Test getter to make sure setter does not affect the boilerplate.
try {
  rec1(0);
} catch (e) {
  assertTrue(e.stack.indexOf("rec1") > 0);
  assertInstanceof(e, RangeError);
}


// Check setting/getting stack property on the prototype chain.
function testErrorPrototype(prototype) {
  var object = {};
  object.__proto__ = prototype;
  // Setting stack property overwrites prototype.stack value.
  object.stack = "123";
  assertEquals(prototype.stack, object.stack);
  assertEquals("123", object.stack);
}

try {
  rec1(0);
} catch (e) {
  e.stack;
  testErrorPrototype(e);
}

try {
  rec1(0);
} catch (e) {
  testErrorPrototype(e);
}

try {
  throw new Error();
} catch (e) {
  testErrorPrototype(e);
}

Error.stackTraceLimit = 3;
try {
  rec1(0);
} catch (e) {
  assertEquals(4, e.stack.split('\n').length);
}

Error.stackTraceLimit = 25.9;
try {
  rec1(0);
} catch (e) {
  assertEquals(26, e.stack.split('\n').length);
}

Error.stackTraceLimit = NaN;
try {
  rec1(0);
} catch (e) {
  assertEquals(1, e.stack.split('\n').length);
}

// A limit outside the range of integers.
Error.stackTraceLimit = 1e12;
try {
  rec1(0);
} catch (e) {
  assertTrue(e.stack.split('\n').length > 100);
}

Error.stackTraceLimit = Infinity;
try {
  rec1(0);
} catch (e) {
  assertTrue(e.stack.split('\n').length > 100);
}

Error.stackTraceLimit = "not a number";
try {
  rec1(0);
} catch (e) {
  assertEquals(undefined, e.stack);
  e.stack = "abc";
  assertEquals("abc", e.stack);
}

Error.stackTraceLimit = 3;
Error = "";  // Overwrite Error in the global object.
try {
  rec1(0);
} catch (e) {
  assertEquals(4, e.stack.split('\n').length);
}
