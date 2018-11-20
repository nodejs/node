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

// Number
assertTrue(typeof 0 == 'number');
assertTrue(typeof 0 === 'number');
assertTrue(typeof 1.2 == 'number');
assertTrue(typeof 1.2 === 'number');
assertFalse(typeof 'x' == 'number');
assertFalse(typeof 'x' === 'number');

// String
assertTrue(typeof 'x' == 'string');
assertTrue(typeof 'x' === 'string');
assertTrue(typeof ('x' + 'x') == 'string');
assertTrue(typeof ('x' + 'x') === 'string');
assertFalse(typeof 1 == 'string');
assertFalse(typeof 1 === 'string');
assertFalse(typeof Object() == 'string');
assertFalse(typeof Object() === 'string');

// Boolean
assertTrue(typeof true == 'boolean');
assertTrue(typeof true === 'boolean');
assertTrue(typeof false == 'boolean');
assertTrue(typeof false === 'boolean');
assertFalse(typeof 1 == 'boolean');
assertFalse(typeof 1 === 'boolean');
assertFalse(typeof Object() == 'boolean');
assertFalse(typeof Object() === 'boolean');

// Undefined
assertTrue(typeof void 0 == 'undefined');
assertTrue(typeof void 0 === 'undefined');
assertFalse(typeof 1 == 'undefined');
assertFalse(typeof 1 === 'undefined');
assertFalse(typeof Object() == 'undefined');
assertFalse(typeof Object() === 'undefined');

// Function
assertTrue(typeof Object == 'function');
assertTrue(typeof Object === 'function');
assertFalse(typeof 1 == 'function');
assertFalse(typeof 1 === 'function');
assertFalse(typeof Object() == 'function');
assertFalse(typeof Object() === 'function');

// Object
assertTrue(typeof Object() == 'object');
assertTrue(typeof Object() === 'object');
assertTrue(typeof new String('x') == 'object');
assertTrue(typeof new String('x') === 'object');
assertTrue(typeof ['x'] == 'object');
assertTrue(typeof ['x'] === 'object');
assertTrue(typeof null == 'object');
assertTrue(typeof null === 'object');
assertFalse(typeof 1 == 'object');
assertFalse(typeof 1 === 'object');
assertFalse(typeof 'x' == 'object');  // bug #674753
assertFalse(typeof 'x' === 'object');
assertFalse(typeof Object == 'object');
assertFalse(typeof Object === 'object');
