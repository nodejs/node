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

// Flags: --allow-natives-syntax

function L0() {
  return %_ArgumentsLength();
}

function L1(a) {
  return %_ArgumentsLength();
}

function L5(a,b,c,d,e) {
  return %_ArgumentsLength();
}


assertEquals(0, L0());
assertEquals(1, L0(1));
assertEquals(2, L0(1,2));
assertEquals(5, L0(1,2,3,4,5));

assertEquals(0, L1());
assertEquals(1, L1(1));
assertEquals(2, L1(1,2));
assertEquals(5, L1(1,2,3,4,5));

assertEquals(0, L5());
assertEquals(1, L5(1));
assertEquals(2, L5(1,2));
assertEquals(5, L5(1,2,3,4,5));


function A(key) {
  return %_Arguments(key);
}

// Integer access.
assertEquals(0, A(0));
assertEquals(0, A(0,1));
assertEquals(2, A(1,2));
assertEquals(2, A(1,2,3,4,5));
assertEquals(5, A(4,2,3,4,5));
assertTrue(typeof A(1) == 'undefined');
assertTrue(typeof A(3,2,1) == 'undefined');

// Out-of-bounds integer access with and without argument
// adaptor frames.
assertTrue(typeof(A(-10000)) == 'undefined');
assertTrue(typeof(A(-10000, 0)) == 'undefined');
assertTrue(typeof(A(-1)) == 'undefined');
assertTrue(typeof(A(-1, 0)) == 'undefined');
assertTrue(typeof(A(10000)) == 'undefined');
assertTrue(typeof(A(10000, 0)) == 'undefined');

// String access.
assertEquals(0, A('0'));
assertEquals(0, A('0',1));
assertEquals(2, A('1',2));
assertEquals(2, A('1',2,3,4,5));
assertEquals(5, A('4',2,3,4,5));
assertTrue(typeof A('1') == 'undefined');
assertTrue(typeof A('3',2,1) == 'undefined');
assertEquals(A, A('callee'));
assertEquals(1, A('length'));
assertEquals(2, A('length',2));
assertEquals(5, A('length',2,3,4,5));
assertEquals({}.toString, A('toString'));
assertEquals({}.isPrototypeOf, A('isPrototypeOf'));
assertTrue(typeof A('xxx') == 'undefined');

// Object access.
function O(key) {
  return { toString: function() { return key; } };
}

assertEquals(0, A(O(0)));
assertEquals(0, A(O(0),1));
assertEquals(2, A(O(1),2));
assertEquals(2, A(O(1),2,3,4,5));
assertEquals(5, A(O(4),2,3,4,5));
assertTrue(typeof A(O(1)) == 'undefined');
assertTrue(typeof A(O(3),2,1) == 'undefined');

assertEquals(0, A(O('0')));
assertEquals(0, A(O('0'),1));
assertEquals(2, A(O('1'),2));
assertEquals(2, A(O('1'),2,3,4,5));
assertEquals(5, A(O('4'),2,3,4,5));
assertTrue(typeof A(O('1')) == 'undefined');
assertTrue(typeof A(O('3'),2,1) == 'undefined');
assertEquals(A, A(O('callee')));
assertEquals(1, A(O('length')));
assertEquals(2, A(O('length'),2));
assertEquals(5, A(O('length'),2,3,4,5));
assertEquals({}.toString, A(O('toString')));
assertEquals({}.isPrototypeOf, A(O('isPrototypeOf')));
assertTrue(typeof A(O('xxx')) == 'undefined');

// Make sure that out-of-bounds access do lookups in the
// prototype chain.
Object.prototype[5] = 42;
assertEquals(42, A(5));
Object.prototype[-5] = 87;
assertEquals(87, A(-5));
