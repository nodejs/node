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

// Normal objects.
// Check for objects.
assertTrue({x:12}.hasOwnProperty('x'));
assertFalse({x:12}.hasOwnProperty('y'));

// Check for strings.
assertTrue(''.hasOwnProperty('length'));
assertTrue(Object.prototype.hasOwnProperty.call('', 'length'));

// Check for numbers.
assertFalse((123).hasOwnProperty('length'));
assertFalse(Object.prototype.hasOwnProperty.call(123, 'length'));

// Frozen object.
// Check for objects.
assertTrue(Object.freeze({x:12}).hasOwnProperty('x'));
assertFalse(Object.freeze({x:12}).hasOwnProperty('y'));

// Check for strings.
assertTrue(Object.freeze('').hasOwnProperty('length'));
assertTrue(Object.prototype.hasOwnProperty.call(Object.freeze(''), 'length'));

// Check for numbers.
assertFalse(Object.freeze(123).hasOwnProperty('length'));
assertFalse(Object.prototype.hasOwnProperty.call(Object.freeze(123), 'length'));

// Direct vs. inherited properties.
var o = ['exist'];
Object.freeze(o);
assertTrue(o.hasOwnProperty('0'));
assertFalse(o.hasOwnProperty('toString'));
assertFalse(o.hasOwnProperty('hasOwnProperty'));

// Using hasOwnProperty as a property name.
var foo = ['a'];
foo.hasOwnProperty = function() { return false; };
Object.freeze(foo);
assertFalse(foo.hasOwnProperty('0')); // always returns false.

// Use another Object's hasOwnProperty
// and call it with 'this' set to foo.
assertTrue(({}).hasOwnProperty.call(foo, '0'));

// It's also possible to use the hasOwnProperty property
// from the Object prototype for this purpose.
assertTrue(Object.prototype.hasOwnProperty.call(foo, '0'));

// Check for null or undefined
var o = Object.freeze([, null, undefined, 'a', 1, Symbol('2')]);
assertFalse(o.hasOwnProperty('0')); // hole
assertTrue(o.hasOwnProperty('1'));
assertTrue(o.hasOwnProperty('2'));
assertTrue(o.hasOwnProperty('3'));
assertTrue(o.hasOwnProperty('4'));
assertTrue(o.hasOwnProperty('5'));
assertFalse(o.hasOwnProperty('6')); // out of bounds

// Sealed object.
// Check for objects.
assertTrue(Object.seal({x:12}).hasOwnProperty('x'));
assertFalse(Object.seal({x:12}).hasOwnProperty('y'));

// Check for strings.
assertTrue(Object.seal('').hasOwnProperty('length'));
assertTrue(Object.prototype.hasOwnProperty.call(Object.seal(''), 'length'));

// Check for numbers.
assertFalse(Object.seal(123).hasOwnProperty('length'));
assertFalse(Object.prototype.hasOwnProperty.call(Object.seal(123), 'length'));

// Direct vs. inherited properties.
var o = ['exist'];
Object.seal(o);
assertTrue(o.hasOwnProperty('0'));
assertFalse(o.hasOwnProperty('toString'));
assertFalse(o.hasOwnProperty('hasOwnProperty'));

// Using hasOwnProperty as a property name.
var foo = ['a'];
foo.hasOwnProperty = function() { return false; };
Object.seal(foo);
assertFalse(foo.hasOwnProperty('0')); // always returns false.

// Use another Object's hasOwnProperty
// and call it with 'this' set to foo.
assertTrue(({}).hasOwnProperty.call(foo, '0'));

// It's also possible to use the hasOwnProperty property
// from the Object prototype for this purpose.
assertTrue(Object.prototype.hasOwnProperty.call(foo, '0'));

// Check for null or undefined
var o = Object.seal([, null, undefined, 'a', 1, Symbol('2')]);
assertFalse(o.hasOwnProperty('0')); // hole.
assertTrue(o.hasOwnProperty('1'));
assertTrue(o.hasOwnProperty('2'));
assertTrue(o.hasOwnProperty('3'));
assertTrue(o.hasOwnProperty('4'));
assertTrue(o.hasOwnProperty('5'));
assertFalse(o.hasOwnProperty('6')); // out of bounds.

// Non-extensible object.
// Check for objects.
assertTrue(Object.preventExtensions({x:12}).hasOwnProperty('x'));
assertFalse(Object.preventExtensions({x:12}).hasOwnProperty('y'));

// Check for strings.
assertTrue(Object.preventExtensions('').hasOwnProperty('length'));
assertTrue(Object.prototype.hasOwnProperty.call(Object.preventExtensions(''), 'length'));

// Check for numbers.
assertFalse(Object.preventExtensions(123).hasOwnProperty('length'));
assertFalse(Object.prototype.hasOwnProperty.call(Object.preventExtensions(123), 'length'));

// Direct vs. inherited properties.
var o = ['exist'];
Object.preventExtensions(o);
assertTrue(o.hasOwnProperty('0'));
assertFalse(o.hasOwnProperty('toString'));
assertFalse(o.hasOwnProperty('hasOwnProperty'));

// Using hasOwnProperty as a property name.
var foo = ['a'];
foo.hasOwnProperty = function() { return false; };
Object.preventExtensions(foo);
assertFalse(foo.hasOwnProperty('0')); // always returns false.

// Use another Object's hasOwnProperty
// and call it with 'this' set to foo.
assertTrue(({}).hasOwnProperty.call(foo, '0'));

// It's also possible to use the hasOwnProperty property
// from the Object prototype for this purpose.
assertTrue(Object.prototype.hasOwnProperty.call(foo, '0'));

// Check for null or undefined.
var o = Object.preventExtensions([, null, undefined, 'a', 1, Symbol('2')]);
assertFalse(o.hasOwnProperty('0')); // hole.
assertTrue(o.hasOwnProperty('1'));
assertTrue(o.hasOwnProperty('2'));
assertTrue(o.hasOwnProperty('3'));
assertTrue(o.hasOwnProperty('4'));
assertTrue(o.hasOwnProperty('5'));
assertFalse(o.hasOwnProperty('6')); // out of bounds.
