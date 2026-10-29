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

"use strict";

function props(x) {
  var array = [];
  for (let p in x) array.push(p);
  return array.sort();
}

assertEquals(0, props({}).length);
assertEquals(1, props({x:1}).length);
assertEquals(2, props({x:1, y:2}).length);

assertArrayEquals(["x"], props({x:1}));
assertArrayEquals(["x", "y"], props({x:1, y:2}));
assertArrayEquals(["x", "y", "zoom"], props({x:1, y:2, zoom:3}));

assertEquals(0, props([]).length);
assertEquals(1, props([1]).length);
assertEquals(2, props([1,2]).length);

assertArrayEquals(["0"], props([1]));
assertArrayEquals(["0", "1"], props([1,2]));
assertArrayEquals(["0", "1", "2"], props([1,2,3]));

var o = {};
var a = [];
let i = "outer_i";
let s = "outer_s";
for (let i = 0x0020; i < 0x01ff; i+=2) {
  let s = 'char:' + String.fromCharCode(i);
  a.push(s);
  o[s] = i;
}
assertArrayEquals(a, props(o));
assertEquals(i, "outer_i");
assertEquals(s, "outer_s");

var a = [];
assertEquals(0, props(a).length);
a[Math.pow(2,30)-1] = 0;
assertEquals(1, props(a).length);
a[Math.pow(2,31)-1] = 0;
assertEquals(2, props(a).length);
a[1] = 0;
assertEquals(3, props(a).length);

var result = '';
for (let p in {a : [0], b : 1}) { result += p; }
assertEquals('ab', result);

var result = '';
for (let p in {a : {v:1}, b : 1}) { result += p; }
assertEquals('ab', result);

var result = '';
for (let p in { get a() {}, b : 1}) { result += p; }
assertEquals('ab', result);

var result = '';
for (let p in { get a() {}, set a(x) {}, b : 1}) { result += p; }
assertEquals('ab', result);


// Check that there is exactly one variable without initializer
// in a for-in statement with let variables.
assertThrows("function foo() { 'use strict'; for (let in {}) { } }", SyntaxError);
assertThrows("function foo() { 'use strict'; for (let x = 3 in {}) { } }", SyntaxError);
assertThrows("function foo() { 'use strict'; for (let x, y in {}) { } }", SyntaxError);
assertThrows("function foo() { 'use strict'; for (let x = 3, y in {}) { } }", SyntaxError);
assertThrows("function foo() { 'use strict'; for (let x, y = 4 in {}) { } }", SyntaxError);
assertThrows("function foo() { 'use strict'; for (let x = 3, y = 4 in {}) { } }", SyntaxError);


// In a normal for statement the iteration variable is
// freshly allocated for each iteration.
function closures1() {
  let a = [];
  for (let i = 0; i < 5; ++i) {
    a.push(function () { return i; });
  }
  for (let j = 0; j < 5; ++j) {
    assertEquals(j, a[j]());
  }
}
closures1();


function closures2() {
  let a = [], b = [];
  for (let i = 0, j = 10; i < 5; ++i, ++j) {
    a.push(function () { return i; });
    b.push(function () { return j; });
  }
  for (let k = 0; k < 5; ++k) {
    assertEquals(k, a[k]());
    assertEquals(k + 10, b[k]());
  }
}
closures2();


function closure_in_for_init() {
  let a = [];
  for (let i = 0, f = function() { return i }; i < 5; ++i) {
    a.push(f);
  }
  for (let k = 0; k < 5; ++k) {
    assertEquals(0, a[k]());
  }
}
closure_in_for_init();


function closure_in_for_cond() {
  let a = [];
  for (let i = 0; a.push(function () { return i; }), i < 5; ++i) { }
  for (let k = 0; k < 5; ++k) {
    assertEquals(k, a[k]());
  }
}
closure_in_for_cond();


function closure_in_for_next() {
  let a = [];
  for (let i = 0; i < 5; a.push(function () { return i; }), ++i) { }
  for (let k = 0; k < 5; ++k) {
    assertEquals(k + 1, a[k]());
  }
}
closure_in_for_next();


// In a for-in statement the iteration variable is fresh
// for each iteration.
function closures3(x) {
  let a = [];
  for (let p in x) {
    a.push(function () { return p; });
  }
  let k = 0;
  for (let q in x) {
    assertEquals(q, a[k]());
    ++k;
  }
}
closures3({a : [0], b : 1, c : {v : 1}, get d() {}, set e(x) {}});

// Check normal for statement completion values.
assertEquals(1, eval("for (let i = 0; i < 10; i++) { 1; }"));
assertEquals(9, eval("for (let i = 0; i < 10; i++) { i; }"));
assertEquals(undefined, eval("for (let i = 0; false;) { }"));
assertEquals(undefined, eval("for (const i = 0; false;) { }"));
assertEquals(undefined, eval("for (let i = 0; i < 10; i++) { }"));
assertEquals(undefined, eval("for (let i = 0; false;) { i; }"));
assertEquals(undefined, eval("for (const i = 0; false;) { i; }"));
assertEquals(undefined, eval("for (let i = 0; true;) { break; }"));
assertEquals(undefined, eval("for (const i = 0; true;) { break; }"));
assertEquals(undefined, eval("for (let i = 0; i < 10; i++) { continue; }"));
assertEquals(undefined, eval("for (let i = 0; true;) { break; i; }"));
assertEquals(undefined, eval("for (const i = 0; true;) { break; i; }"));
assertEquals(undefined, eval("for (let i = 0; i < 10; i++) { continue; i; }"));
assertEquals(0, eval("for (let i = 0; true;) { i; break; }"));
assertEquals(0, eval("for (const i = 0; true;) { i; break; }"));
assertEquals(9, eval("for (let i = 0; i < 10; i++) { i; continue; }"));
assertEquals(undefined,
  eval("for (let i = 0; true; i++) { i; if (i >= 3) break; }"));
assertEquals(3,
  eval("for (let i = 0; true; i++) { i; if (i >= 3) { i; break; } }"));
assertEquals(undefined,
  eval("for (let i = 0; true; i++) { if (i >= 3) break; i; }"));
assertEquals(3,
  eval("for (let i = 0; true; i++) { if (i >= 3) { i; break; }; i; }"));
assertEquals(undefined,
  eval("for (let i = 0; i < 10; i++) { if (i >= 3) continue; i; }"));
assertEquals(9,
  eval("for (let i = 0; i < 10; i++) { if (i >= 3) {i; continue; }; i; }"));
assertEquals(undefined, eval("foo: for (let i = 0; true;) { break foo; }"));
assertEquals(undefined, eval("foo: for (const i = 0; true;) { break foo; }"));
assertEquals(3, eval("foo: for (let i = 3; true;) { i; break foo; }"));
