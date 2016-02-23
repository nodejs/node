// Copyright 2013 the V8 project authors. All rights reserved.
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

var a = {};
a.x = 1
a.y = 1.5

var b = {}
b.x = 1.5;
b.y = 1;

var c = {}
c.x = 1.5;

var d = {}
d.x = 1.5;

var e = {}
e.x = 1.5;

var f = {}
f.x = 1.5;

var g = {}
g.x = 1.5;

var h = {}
h.x = 1.5;

var i = {}
i.x = 1.5;

var o = {}
var p = {y : 10, z : 1}
o.__proto__ = p;
delete p.z

function foo(v, w) {
  // Make load via IC in optimized code. Its target will get overwritten by
  // lazy deopt patch for the stack check.
  v.y;
  // Make store with transition to make this code dependent on the map.
  w.y = 1;
  return b.y;
}

foo(o, c);
foo(o, d);
foo(o, e);
%OptimizeFunctionOnNextCall(foo);
foo(b, f);
foo(b, g);
foo(b, h);
foo(a, i);
