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

// Flags: --allow-natives-syntax

// Test stack traces with method calls.
function Hest() {}
function Svin() {}

Svin.prototype.two = function() { /* xxxxxxx */ o.three(); }

Hest.prototype.one = function(x) { x.two(); }

Hest.prototype.three = function() { if (v == 42) throw new Error("urg"); }

var o = new Hest();
var s = new Svin();
var v = 0;

%PrepareFunctionForOptimization(Hest.prototype.one);
for (var i = 0; i < 5; i++) {
  o.one(s);
}
%OptimizeFunctionOnNextCall(Hest.prototype.one);
o.one(s);
%PrepareFunctionForOptimization(Hest.prototype.three);
for (var i = 0; i < 5; i++) {
  o.one(s);
}
%OptimizeFunctionOnNextCall(Hest.prototype.three);
o.one(s);

v = 42;

try {
  o.one(s);
} catch (e) {
  var stack = e.stack.toString();
  var p3 = stack.indexOf("at Hest.three");
  var p2 = stack.indexOf("at Svin.two");
  var p1 = stack.indexOf("at Hest.one");
  assertTrue(p3 != -1);
  assertTrue(p2 != -1);
  assertTrue(p1 != -1);
  assertTrue(p3 < p2);
  assertTrue(p2 < p1);
  assertTrue(stack.indexOf("38:56") != -1);
  assertTrue(stack.indexOf("34:51") != -1);
  assertTrue(stack.indexOf("36:38") != -1);
  assertTrue(stack.indexOf("60:5") != -1);
}
