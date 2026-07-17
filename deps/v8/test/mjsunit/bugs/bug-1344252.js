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

// Test that setter accessors added to the prototype chain are called
// when setting properties.

// Test that accessors added to the prototype chain are called
// eventhough there are inline caches for setting the property

function F() {
  this.x = 42;
  this.y = 87;
}

// Force the inline caches to monomorphic state.
new F(); new F();

// Add a setter for x to Object.prototype and make sure it gets
// called.
var result_x;
Object.prototype.__defineSetter__('x', function(value) { result_x = value; });
var f = new F();
assertEquals(42, result_x);
assertTrue(typeof f.x == 'undefined');

// Add a setter for y by changing the prototype of f and make sure
// that gets called too.
var result_y;
var proto = new Object();
proto.__defineSetter__('y', function (value) { result_y = value; });
var f = new F();
f.y = undefined;
f.__proto__ = proto;
F.call(f);
assertEquals(87, result_y);
assertTrue(typeof f.y == 'undefined');


// Test the same issue in the runtime system.  Make sure that
// accessors added to the prototype chain are called instead of
// following map transitions.
//
// Create two objects.
var result_z;
var o1 = new Object();
var o2 = new Object();
// Add a z property to o1 to create a map transition.
o1.z = 32;
// Add a z accessor in the prototype chain for o1 and o2.
Object.prototype.__defineSetter__('z', function(value) { result_z = value; });
// The accessor should be called for o2.
o2.z = 27;
assertEquals(27, result_z);
assertTrue(typeof o2.z == 'undefined');
