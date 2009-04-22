// Copyright 2008 the V8 project authors. All rights reserved.
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

// Flags: --expose-debug-as debug
// Get the Debug object exposed from the debug context global object.
Debug = debug.Debug

// Simple object.
var a = {};

// Create mirror for the object.
var mirror = debug.MakeMirror(a);

// Initially one reference from the global object.
assertEquals(1, mirror.referencedBy().length);
assertEquals(1, mirror.referencedBy(0).length);
assertEquals(1, mirror.referencedBy(1).length);
assertEquals(1, mirror.referencedBy(10).length);

// Add some more references from simple objects and arrays.
var b = {}
b.a = a;
assertEquals(2, mirror.referencedBy().length);
var c = {}
c.a = a;
c.aa = a;
c.aaa = a;
assertEquals(3, mirror.referencedBy().length);
function d(){};
d.a = a
assertEquals(4, mirror.referencedBy().length);
e = [a,b,c,d];
assertEquals(5, mirror.referencedBy().length);


// Simple closure.
function closure_simple(p) {
  return function() { p = null; };
}

// This adds a reference (function context).
f = closure_simple(a);
assertEquals(6, mirror.referencedBy().length);
// This clears the reference (in function context).
f()
assertEquals(5, mirror.referencedBy().length);

// Use closure with eval - creates arguments array.
function closure_eval(p, s) {
  if (s) {
    eval(s);
  }
  return function e(s) { eval(s); };
}

// This adds a references (function context).
g = closure_eval(a);
assertEquals(6, mirror.referencedBy().length);

// Dynamically create a variable. This should create a context extension.
h = closure_eval(null, "var x_");
assertEquals(6, mirror.referencedBy().length);
// Adds a reference when set.
h("x_ = a");
var x = mirror.referencedBy();
assertEquals(7, mirror.referencedBy().length);
// Removes a reference when cleared.
h("x_ = null");
assertEquals(6, mirror.referencedBy().length);

// Check circular references.
x = {}
mirror = debug.MakeMirror(x);
assertEquals(1, mirror.referencedBy().length);
x.x = x;
assertEquals(2, mirror.referencedBy().length);
x = null;
assertEquals(0, mirror.referencedBy().length);

// Check many references.
y = {}
mirror = debug.MakeMirror(y);
refs = [];
for (var i = 0; i < 200; i++) {
  refs[i] = {'y': y};
}
y = null;
assertEquals(200, mirror.referencedBy().length);
