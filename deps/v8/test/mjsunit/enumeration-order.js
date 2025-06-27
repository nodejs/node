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

function check_enumeration_order(obj)  {
  var value = 0;
  for (var name in obj) assertTrue(value < obj[name]);
  value = obj[name];
}

function make_object(size)  {
  var a = new Object();

  for (var i = 0; i < size; i++) a["a_" + i] = i + 1;
  check_enumeration_order(a);

  for (var i = 0; i < size; i +=3) delete a["a_" + i];
  check_enumeration_order(a);
}

// Validate the enumeration order for object up to 100 named properties.
for (var j = 1; j< 100; j++) make_object(j);


function make_literal_object(size)  {
  var code = "{ ";
  for (var i = 0; i < size-1; i++) code += " a_" + i + " : " + (i + 1) + ", ";
  code += "a_" + (size - 1) + " : " + size;
  code += " }";
  eval("var a = " + code);
  check_enumeration_order(a);
}

// Validate the enumeration order for object literals up to 100 named
// properties.
for (var j = 1; j< 100; j++) make_literal_object(j);

// We enumerate indexed properties in numerical order followed by
// named properties in insertion order, followed by indexed properties
// of the prototype object in numerical order, followed by named
// properties of the prototype object in insertion order, and so on.
//
// This enumeration order is not required by the specification, so
// this just documents our choice.
var proto2 = {};
proto2[140000] = 0;
proto2.a = 0;
proto2[2] = 0;
proto2[3] = 0;  // also on the 'proto1' object
proto2.b = 0;
proto2[4294967294] = 0;
proto2.c = 0;
proto2[4294967295] = 0;

var proto1 = {};
proto1[5] = 0;
proto1.d = 0;
proto1[3] = 0;
proto1.e = 0;
proto1.f = 0;  // also on the 'o' object

var o = {};
o[-23] = 0;
o[300000000000] = 0;
o[23] = 0;
o.f = 0;
o.g = 0;
o[-4] = 0;
o[42] = 0;

o.__proto__ = proto1;
proto1.__proto__ = proto2;

var expected = ['23', '42',  // indexed from 'o'
                '-23', '300000000000', 'f', 'g', '-4',  // named from 'o'
                '3', '5',  // indexed from 'proto1'
                'd', 'e',  // named from 'proto1'
                '2', '140000', '4294967294',  // indexed from 'proto2'
                'a', 'b', 'c', '4294967295'];  // named from 'proto2'
var actual = [];
for (var p in o) actual.push(p);
assertArrayEquals(expected, actual);
