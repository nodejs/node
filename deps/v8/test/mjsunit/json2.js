// Copyright 2012 the V8 project authors. All rights reserved.
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

// Test JSON.stringify on the global object.
var a = 12345;
assertTrue(JSON.stringify(this).indexOf('"a":12345') > 0);

// Test JSON.stringify of array in dictionary mode.
var array_1 = [];
var array_2 = [];
array_1[100000] = 1;
array_2[100000] = function() { return 1; };
var nulls = "";
for (var i = 0; i < 100000; i++) {
  nulls += 'null,';
}
expected_1 = '[' + nulls + '1]';
expected_2 = '[' + nulls + 'null]';
assertEquals(expected_1, JSON.stringify(array_1));
assertEquals(expected_2, JSON.stringify(array_2));

// Test JSValue with custom prototype.
var num_wrapper = Object(42);
num_wrapper.__proto__ = { __proto__: null,
                          toString: function() { return true; } };
assertEquals('1', JSON.stringify(num_wrapper));

var str_wrapper = Object('2');
str_wrapper.__proto__ = { __proto__: null,
                          toString: function() { return true; } };
assertEquals('"true"', JSON.stringify(str_wrapper));

var bool_wrapper = Object(false);
bool_wrapper.__proto__ = { __proto__: null,
                           toString: function() { return true; } };
// Note that toString function is not evaluated here!
assertEquals('false', JSON.stringify(bool_wrapper));

// Test getters.
var counter = 0;
var getter_obj = { get getter() {
                         counter++;
                         return 123;
                       } };
assertEquals('{"getter":123}', JSON.stringify(getter_obj));
assertEquals(1, counter);

// Test toJSON function.
var tojson_obj = { toJSON: function() {
                             counter++;
                             return [1, 2];
                           },
                   a: 1};
assertEquals('[1,2]', JSON.stringify(tojson_obj));
assertEquals(2, counter);

// Test that we don't recursively look for the toJSON function.
var tojson_proto_obj = { a: 'fail' };
tojson_proto_obj.__proto__ = { toJSON: function() {
                                         counter++;
                                         return tojson_obj;
                                       } };
assertEquals('{"a":1}', JSON.stringify(tojson_proto_obj));

// Test toJSON produced by a getter.
var tojson_via_getter = { get toJSON() {
                                return function(x) {
                                         counter++;
                                         return 321;
                                       };
                              },
                          a: 1 };
assertEquals('321', JSON.stringify(tojson_via_getter));

// Test toJSON with key.
tojson_obj = { toJSON: function(key) { return key + key; } };
var tojson_with_key_1 = { a: tojson_obj, b: tojson_obj };
assertEquals('{"a":"aa","b":"bb"}', JSON.stringify(tojson_with_key_1));
var tojson_with_key_2 = [ tojson_obj, tojson_obj ];
assertEquals('["00","11"]', JSON.stringify(tojson_with_key_2));

// Test toJSON with exception.
var tojson_ex = { toJSON: function(key) { throw "123" } };
assertThrows(function() { JSON.stringify(tojson_ex); });

// Test toJSON with access to this.
var obj = { toJSON: function(key) { return this.a + key; }, a: "x" };
assertEquals('{"y":"xy"}', JSON.stringify({y: obj}));

// Test holes in arrays.
var fast_smi = [1, 2, 3, 4];
fast_smi.__proto__ = [7, 7, 7, 7];
delete fast_smi[2];
assertTrue(%HasFastSmiElements(fast_smi));
assertEquals("[1,2,7,4]", JSON.stringify(fast_smi));

var fast_double = [1.1, 2, 3, 4];
fast_double.__proto__ = [7, 7, 7, 7];

delete fast_double[2];
assertTrue(%HasFastDoubleElements(fast_double));
assertEquals("[1.1,2,7,4]", JSON.stringify(fast_double));

var fast_obj = [1, 2, {}, {}];
fast_obj.__proto__ = [7, 7, 7, 7];

delete fast_obj[2];
assertTrue(%HasFastObjectElements(fast_obj));
assertEquals("[1,2,7,{}]", JSON.stringify(fast_obj));

var getter_side_effect = { a: 1,
                           get b() {
                             delete this.a;
                             delete this.c;
                             this.e = 5;
                             return 2;
                           },
                           c: 3,
                           d: 4 };
assertEquals('{"a":1,"b":2,"d":4}', JSON.stringify(getter_side_effect));
assertEquals('{"b":2,"d":4,"e":5}', JSON.stringify(getter_side_effect));

var non_enum = {};
non_enum.a = 1;
Object.defineProperty(non_enum, "b", { value: 2, enumerable: false });
non_enum.c = 3;
assertEquals('{"a":1,"c":3}', JSON.stringify(non_enum));
