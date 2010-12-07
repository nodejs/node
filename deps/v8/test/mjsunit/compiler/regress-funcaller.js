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

// Test function.caller.
function A() {}

function fun(x) {
  if (x == 0) return fun.caller;
  if (x == 1) return gee.caller;
  return 42;
}
function gee(x) { return this.f(x); }

A.prototype.f = fun;
A.prototype.g = gee;

var o = new A();

for (var i=0; i<5000000; i++) {
  o.g(i);
}
assertEquals(gee, o.g(0));
assertEquals(null, o.g(1));

// Test when called from another function.
function hej(x) {
  if (x == 0) return o.g(x);
  if (x == 1) return o.g(x);
  return o.g(x);
}

for (var j=0; j<5000000; j++) {
  hej(j);
}
assertEquals(gee, hej(0));
assertEquals(hej, hej(1));

// Test when called from eval.
function from_eval(x) {
  if (x == 0) return eval("o.g(x);");
  if (x == 1) return eval("o.g(x);");
  return o.g(x);
}

for (var j=0; j<5000000; j++) {
  from_eval(j);
}
assertEquals(gee, from_eval(0));
assertEquals(from_eval, from_eval(1));
