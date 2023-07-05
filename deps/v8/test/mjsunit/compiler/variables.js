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

// Simple tests of the various kinds of variable references in the
// implementstion.

// Global variables.
var x = 0;
function f0() { return x; }
assertEquals(0, f0());


// Parameters.
function f1(x) { return x; }
assertEquals(1, f1(1));


// Stack-allocated locals.
function f2() { var x = 2; return x; }
assertEquals(2, f2());


// Context-allocated locals.  Local function forces x into f3's context.
function f3(x) {
  function g() { return x; }
  return x;
}
assertEquals(3, f3(3));

// Local function reads x from an outer context.
function f4(x) {
  function g() { return x; }
  return g();
}
assertEquals(4, f4(4));


// Lookup slots.  'With' forces x to be looked up at runtime.
function f5(x) {
  with ({}) return x;
}
assertEquals(5, f5(5));


// Parameters rewritten to property accesses.  Using the name 'arguments'
// (even if it shadows the arguments object) forces all parameters to be
// rewritten to explicit property accesses.
function f6(arguments) { return arguments; }
assertEquals(6, f6(6));
