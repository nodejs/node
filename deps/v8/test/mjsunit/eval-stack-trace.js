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

// Return the stack frames of an Error object.
Error.prototype.getFrames = function() {
  Error.prepareStackTrace = function(error, frames) {
    return frames;
  }
  var frames = this.stack;
  Error.prepareStackTrace = undefined;
  return frames;
}

String.prototype.contains = function(pattern) {
  return this.indexOf(pattern) > -1;
}

// Check for every frame that a certain method returns the
// expected value for every frame.
Array.prototype.verifyEquals = function(frames, func_name) {
  this.forEach(
    function(element, index) {
      var frame = frames[index];
      if (element === null) return;
      assertEquals(element, (frame[func_name])());
    }
  );
}

// Check for every frame that a certain method has a return value
// that contains the expected pattern for every frame.
Array.prototype.verifyContains = function(frames, func_name) {
  this.forEach(
    function(element, index) {
      var frame = frames[index];
      if (element === null) return;
      assertTrue((frame[func_name])().contains(element));
    }
  );
}

// Check for every frame that a certain method returns undefined
// when expected.
Array.prototype.verifyUndefined = function(frames, func_name) {
  this.forEach(
    function(element, index) {
      var frame = frames[index];
      if (element === null) return;
      assertEquals(element, (frame[func_name])() === undefined);
    }
  );
}


// Simple eval.
var code1 = "function f() {        \n" +
            "  throw new Error(3); \n" +  // Line 2
            "}                     \n" +
            "f();                  \n";   // Line 4

function g() {
  eval(code1);
}

try {
  g();
} catch (e) {
  // We expect something like
  //   f (eval at g (eval-stack.js:87:8), <anonymous>:2:9)
  //   eval (eval at g (eval-stack.js:87:8), <anonymous>:4:1)
  //   g (eval-stack.js:87:3)
  //   eval-stack.js:94:3
  var frames = e.getFrames();
  assertEquals(4, frames.length);
  ["f", "eval", "g"]
      .verifyEquals(frames, "getFunctionName");
  [2, 4]
      .verifyEquals(frames, "getLineNumber");
  ["<anonymous>:2:", "<anonymous>:4:"]
      .verifyContains(frames, "toString");
  [true, true, false, false]
      .verifyUndefined(frames, "getFileName");
  ["eval at g", "eval at g"]
      .verifyContains(frames, "getEvalOrigin");
}


// Nested eval.
var code2 = "function h() {        \n" +
            "  // Empty            \n" +
            "  eval(code1);        \n" +  // Line 3
            "}                     \n" +
            "h();                  \n";   // Line 5

try {
  eval(code2);
} catch (e) {
  // We expect something like
  //   f (eval at h (eval at <anonymous> (eval-stack.js:116:8)),
  //       <anonymous>:2:9)
  //   eval (eval at h (eval at <anonymous> (eval-stack.js:116:8)),
  //       <anonymous>:4:1)
  //   h (eval at <anonymous> (eval-stack.js:116:8), <anonymous>:3:3)
  //   eval (eval at <anonymous> (eval-stack.js:116:8), <anonymous>:5:1)
  //   eval-stack.js:116:3
  var frames = e.getFrames();
  assertEquals(5, frames.length);
  ["f", "eval", "h", "eval"]
      .verifyEquals(frames, "getFunctionName");
  [2, 4, 3, 5]
      .verifyEquals(frames, "getLineNumber");
  ["<anonymous>:2:", "<anonymous>:4:", "<anonymous>:3:", "<anonymous>:5:"]
      .verifyContains(frames, "toString");
  [true, true, true, true, false]
      .verifyUndefined(frames, "getFileName");
  ["eval at h (eval at <anonymous> (",
   "eval at h (eval at <anonymous> (",
   "eval at <anonymous> (",
   "eval at <anonymous> ("]
      .verifyContains(frames, "getEvalOrigin");
}


// Nested eval calling through non-eval defined function.
var code3 = "function h() {        \n" +
            "  // Empty            \n" +
            "  g();                \n" +  // Line 3
            "}                     \n" +
            "h();                  \n";   // Line 5

try {
  eval(code3);
} catch (e) {
  // We expect something like
  //   f (eval at g (test.js:83:8), <anonymous>:2:9)
  //   eval (eval at g (test.js:83:8), <anonymous>:4:1)
  //   g (test.js:83:3)
  //   h (eval at <anonymous> (test.js:149:8), <anonymous>:3:3)
  //   eval (eval at <anonymous> (test.js:149:8), <anonymous>:5:1)
  //   test.js:149:3
  var frames = e.getFrames();
  assertEquals(6, frames.length);
  ["f", "eval", "g", "h", "eval"]
      .verifyEquals(frames, "getFunctionName");
  [2, 4, null, 3, 5]
      .verifyEquals(frames, "getLineNumber");
  ["<anonymous>:2:", "<anonymous>:4:", null, "<anonymous>:3:", "<anonymous>:5:"]
      .verifyContains(frames, "toString");
  [true, true, false, true, true, false]
      .verifyUndefined(frames, "getFileName");
  ["eval at g (",
   "eval at g (",
   null,
   "eval at <anonymous> (",
   "eval at <anonymous> ("]
      .verifyContains(frames, "getEvalOrigin");
}


// Calling function defined in eval.
eval("function f() {               \n" +
     "  throw new Error(3);        \n" +
     "}                            \n");

try {
  f();
} catch (e) {
  // We expect something like
  //   f (eval at <anonymous> (test.js:182:40), <anonymous>:2:9)
  //   test.js:186:3
  var frames = e.getFrames();
  assertEquals(2, frames.length);
  ["f"].verifyEquals(frames, "getFunctionName");
  [2].verifyEquals(frames, "getLineNumber");
  ["<anonymous>:2:"].verifyContains(frames, "toString");
  [true, false].verifyUndefined(frames, "getFileName");
  ["eval at <anonymous> ("].verifyContains(frames, "getEvalOrigin");
}

