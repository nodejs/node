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

// Flags: --builtins-in-stack-traces


// Poisonous object that throws a reference error if attempted converted to
// a primitive values.
var thrower = { valueOf: function() { FAIL; },
                toString: function() { FAIL; } };

// Tests that a native constructor function is included in the
// stack trace.
function testTraceNativeConstructor(nativeFunc) {
  var nativeFuncName = nativeFunc.name;
  try {
    new nativeFunc(thrower);
    assertUnreachable(nativeFuncName);
  } catch (e) {
    assertTrue(e.stack.indexOf(nativeFuncName) >= 0, nativeFuncName);
  }
}

// Tests that a native conversion function is included in the
// stack trace.
function testTraceNativeConversion(nativeFunc) {
  var nativeFuncName = nativeFunc.name;
  try {
    nativeFunc(thrower);
    assertUnreachable(nativeFuncName);
  } catch (e) {
    assertTrue(e.stack.indexOf(nativeFuncName) >= 0, nativeFuncName);
  }
}


function testNotOmittedBuiltin(throwing, included) {
  try {
    throwing();
    assertUnreachable(included);
  } catch (e) {
    assertTrue(e.stack.indexOf(included) >= 0, included);
  }
}


testTraceNativeConversion(String);  // Does ToString on argument.
testTraceNativeConversion(Number);  // Does ToNumber on argument.
testTraceNativeConversion(RegExp);  // Does ToString on argument.

testTraceNativeConstructor(String);  // Does ToString on argument.
testTraceNativeConstructor(Number);  // Does ToNumber on argument.
testTraceNativeConstructor(RegExp);  // Does ToString on argument.
testTraceNativeConstructor(Date);    // Does ToNumber on argument.

// QuickSort has builtins object as receiver, and is non-native
// builtin. Should not be omitted with the --builtins-in-stack-traces flag.
testNotOmittedBuiltin(function(){ [thrower, 2].sort(function (a,b) {
                                                     (b < a) - (a < b); });
                      }, "QuickSort");

// Not omitted even though ADD from runtime.js is a non-native builtin.
testNotOmittedBuiltin(function(){ thrower + 2; }, "ADD");