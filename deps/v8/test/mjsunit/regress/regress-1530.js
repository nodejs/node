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

// Test that redefining the 'prototype' property of a function object
// does actually set the internal value and does not screw up any
// shadowing between said property and the internal value.

var f = function() {};

// Verify that normal assignment of 'prototype' property works properly
// and updates the internal value.
var a = { foo: 'bar' };
f.prototype = a;
assertSame(f.prototype, a);
assertSame(f.prototype.foo, 'bar');
assertSame(new f().foo, 'bar');
assertSame(Object.getPrototypeOf(new f()), a);
assertSame(Object.getOwnPropertyDescriptor(f, 'prototype').value, a);
assertTrue(Object.getOwnPropertyDescriptor(f, 'prototype').writable);

// Verify that 'prototype' behaves like a data property when it comes to
// redefining with Object.defineProperty() and the internal value gets
// updated.
var b = { foo: 'baz' };
Object.defineProperty(f, 'prototype', { value: b, writable: true });
assertSame(f.prototype, b);
assertSame(f.prototype.foo, 'baz');
assertSame(new f().foo, 'baz');
assertSame(Object.getPrototypeOf(new f()), b);
assertSame(Object.getOwnPropertyDescriptor(f, 'prototype').value, b);
assertTrue(Object.getOwnPropertyDescriptor(f, 'prototype').writable);

// Verify that the previous redefinition didn't screw up callbacks and
// the internal value still gets updated.
var c = { foo: 'other' };
f.prototype = c;
assertSame(f.prototype, c);
assertSame(f.prototype.foo, 'other');
assertSame(new f().foo, 'other');
assertSame(Object.getPrototypeOf(new f()), c);
assertSame(Object.getOwnPropertyDescriptor(f, 'prototype').value, c);
assertTrue(Object.getOwnPropertyDescriptor(f, 'prototype').writable);

// Verify that 'prototype' can be redefined to contain a different value
// and have a different writability attribute at the same time.
var d = { foo: 'final' };
Object.defineProperty(f, 'prototype', { value: d, writable: false });
assertSame(f.prototype, d);
assertSame(f.prototype.foo, 'final');
assertSame(new f().foo, 'final');
assertSame(Object.getPrototypeOf(new f()), d);
assertSame(Object.getOwnPropertyDescriptor(f, 'prototype').value, d);
assertFalse(Object.getOwnPropertyDescriptor(f, 'prototype').writable);

// Verify that non-writability of redefined 'prototype' is respected.
assertThrows("'use strict'; f.prototype = {}");
assertThrows("Object.defineProperty(f, 'prototype', { value: {} })");

// Verify that non-configurability of other properties is respected, but
// non-writability is ignored by Object.defineProperty().
// name and length are configurable in ES6
Object.defineProperty(f, 'name', { value: {} });
Object.defineProperty(f, 'length', { value: {} });
assertThrows("Object.defineProperty(f, 'caller', { value: {} })");
assertThrows("Object.defineProperty(f, 'arguments', { value: {} })");
