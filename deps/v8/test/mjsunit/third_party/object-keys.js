// Copyright (c) 2006 Apple Computer, Inc. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//
// 1. Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following
// disclaimer in the documentation and/or other materials provided
// with the distribution.
//
// 3. Neither the name of the copyright holder(s) nor the names of any
// contributors may be used to endorse or promote products derived
// from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
// FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
// COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
// INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
// HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
// STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
// OF THE POSSIBILITY OF SUCH DAMAGE.

// Based on LayoutTests/fast/js/Object-keys.html

assertEquals(Object.keys(2), []);
assertEquals(Object.keys("foo"), ["0", "1", "2"]);
assertThrows(function () { Object.keys(null) }, TypeError);
assertThrows(function () { Object.keys(undefined) }, TypeError);

assertEquals(Object.keys({}), []);
assertEquals(Object.keys({a:null}), ['a']);
assertEquals(Object.keys({a:null, b:null}), ['a', 'b']);
assertEquals(Object.keys({b:null, a:null}), ['b', 'a']);
assertEquals(Object.keys([]), []);
assertEquals(Object.keys([null]), ['0']);
assertEquals(Object.keys([null,null]), ['0', '1']);
assertEquals(Object.keys([null,null,,,,null]), ['0', '1', '5']);
assertEquals(Object.keys({__proto__:{a:null}}), []);
assertEquals(Object.keys({__proto__:[1,2,3]}), []);
var x = [];
x.__proto__ = [1, 2, 3];
assertEquals(Object.keys(x), []);
assertEquals(Object.keys(function () {}), []);

assertEquals('string', typeof(Object.keys([1])[0]));

function argsTest(a, b, c) {
  assertEquals(['0', '1', '2'], Object.keys(arguments));
}

argsTest(1, 2, 3);

var literal = {a: 1, b: 2, c: 3};
var keysBefore = Object.keys(literal);
assertEquals(['a', 'b', 'c'], keysBefore);
keysBefore[0] = 'x';
var keysAfter = Object.keys(literal);
assertEquals(['a', 'b', 'c'], keysAfter);
assertEquals(['x', 'b', 'c'], keysBefore);
