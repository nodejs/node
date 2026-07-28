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

// Flags: --allow-natives-syntax

function typeofDirectly() {
  return typeof{} === 'undefined';
};
%PrepareFunctionForOptimization(typeofDirectly);
typeofDirectly();
typeofDirectly();
%OptimizeFunctionOnNextCall(typeofDirectly);
typeofDirectly();

function typeofViaVariable() {
  var foo = typeof{};
  return foo === "undefined";
};
%PrepareFunctionForOptimization(typeofViaVariable);
typeofViaVariable();
typeofViaVariable();
%OptimizeFunctionOnNextCall(typeofViaVariable);
typeofViaVariable();

function typeofMinifiedDirectly() {
  return typeof{}>'u';
};
%PrepareFunctionForOptimization(typeofMinifiedDirectly);
typeofMinifiedDirectly();
typeofMinifiedDirectly();
%OptimizeFunctionOnNextCall(typeofMinifiedDirectly);
typeofMinifiedDirectly();

function typeofMinifiedNotUndefinedDirectly() {
  return typeof{}<'u';
};
%PrepareFunctionForOptimization(typeofMinifiedNotUndefinedDirectly);
typeofMinifiedNotUndefinedDirectly();
typeofMinifiedNotUndefinedDirectly();
%OptimizeFunctionOnNextCall(typeofMinifiedNotUndefinedDirectly);
typeofMinifiedNotUndefinedDirectly();

// Test typeof on right side: 'u' < typeof(x) is equivalent to typeof(x) > 'u'
function typeofMinifiedReversed() {
  return 'u'<typeof{};
};
%PrepareFunctionForOptimization(typeofMinifiedReversed);
typeofMinifiedReversed();
typeofMinifiedReversed();
%OptimizeFunctionOnNextCall(typeofMinifiedReversed);
typeofMinifiedReversed();

// Test typeof in if statement (test context) - should use InvertControlFlow
function typeofInIfStatement(x) {
  if (typeof x < 'u') return 1;
  return 0;
};
%PrepareFunctionForOptimization(typeofInIfStatement);
assertEquals(1, typeofInIfStatement("string"));
assertEquals(0, typeofInIfStatement(undefined));
%OptimizeFunctionOnNextCall(typeofInIfStatement);
assertEquals(1, typeofInIfStatement("string"));
assertEquals(0, typeofInIfStatement(undefined));

// Test typeof > 'u' in if statement
function typeofGreaterInIfStatement(x) {
  if (typeof x > 'u') return 1;
  return 0;
};
%PrepareFunctionForOptimization(typeofGreaterInIfStatement);
assertEquals(0, typeofGreaterInIfStatement("string"));
assertEquals(1, typeofGreaterInIfStatement(undefined));
%OptimizeFunctionOnNextCall(typeofGreaterInIfStatement);
assertEquals(0, typeofGreaterInIfStatement("string"));
assertEquals(1, typeofGreaterInIfStatement(undefined));

// Test 'u' > typeof(x) in if statement (equivalent to typeof(x) < 'u')
function typeofReversedInIfStatement(x) {
  if ('u' > typeof x) return 1;
  return 0;
};
%PrepareFunctionForOptimization(typeofReversedInIfStatement);
assertEquals(1, typeofReversedInIfStatement("string"));
assertEquals(0, typeofReversedInIfStatement(undefined));
%OptimizeFunctionOnNextCall(typeofReversedInIfStatement);
assertEquals(1, typeofReversedInIfStatement("string"));
assertEquals(0, typeofReversedInIfStatement(undefined));
