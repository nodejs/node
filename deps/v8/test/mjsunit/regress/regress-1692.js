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

// Test that Object.prototype.propertyIsEnumerable handles array indices
// correctly.

var p = Object.create({}, {
  a : { value : 42, enumerable : true },
  b : { value : 42, enumerable : false },
  1 : { value : 42, enumerable : true },
  2 : { value : 42, enumerable : false },
  f : { get: function(){}, enumerable: true },
  g : { get: function(){}, enumerable: false },
  11 : { get: function(){}, enumerable: true },
  12 : { get: function(){}, enumerable: false }
});
var o = Object.create(p, {
  c : { value : 42, enumerable : true },
  d : { value : 42, enumerable : false },
  3 : { value : 42, enumerable : true },
  4 : { value : 42, enumerable : false },
  h : { get: function(){}, enumerable: true },
  k : { get: function(){}, enumerable: false },
  13 : { get: function(){}, enumerable: true },
  14 : { get: function(){}, enumerable: false }
});

// Inherited properties are ignored.
assertFalse(o.propertyIsEnumerable("a"));
assertFalse(o.propertyIsEnumerable("b"));
assertFalse(o.propertyIsEnumerable("1"));
assertFalse(o.propertyIsEnumerable("2"));

// Own properties.
assertTrue(o.propertyIsEnumerable("c"));
assertFalse(o.propertyIsEnumerable("d"));
assertTrue(o.propertyIsEnumerable("3"));
assertFalse(o.propertyIsEnumerable("4"));

// Inherited accessors.
assertFalse(o.propertyIsEnumerable("f"));
assertFalse(o.propertyIsEnumerable("g"));
assertFalse(o.propertyIsEnumerable("11"));
assertFalse(o.propertyIsEnumerable("12"));

// Own accessors.
assertTrue(o.propertyIsEnumerable("h"));
assertFalse(o.propertyIsEnumerable("k"));
assertTrue(o.propertyIsEnumerable("13"));
assertFalse(o.propertyIsEnumerable("14"));

// Nonexisting properties.
assertFalse(o.propertyIsEnumerable("xxx"));
assertFalse(o.propertyIsEnumerable("999"));

// String object properties.
var o = Object("string");
// Non-string property on String object.
o[10] = 42;
assertTrue(o.propertyIsEnumerable(10));
assertTrue(o.propertyIsEnumerable(0));

// Fast elements.
var o = [1,2,3,4,5];
assertTrue(o.propertyIsEnumerable(3));
