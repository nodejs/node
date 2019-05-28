// Copyright 2013 the V8 project authors. All rights reserved.
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

// Flags: --track-fields --track-double-fields --allow-natives-syntax
// Flags: --modify-field-representation-inplace

// Test transitions caused by changes to field representations.

function create_smi_object() {
  var o = {};
  o.x = 1;
  o.y = 2;
  o.z = 3;
  return o;
}

var o1 = create_smi_object();
var o2 = create_smi_object();

// o1,o2 are smi, smi, smi
assertTrue(%HaveSameMap(o1, o2));
o1.y = 1.3;
// o1 is smi, double, smi
assertFalse(%HaveSameMap(o1, o2));
o2.y = 1.5;
// o2 is smi, double, smi
assertTrue(%HaveSameMap(o1, o2));

// o3 is initialized as smi, double, smi
var o3 = create_smi_object();
assertTrue(%HaveSameMap(o1, o3));

function set_large(o, v) {
  o.x01 = v; o.x02 = v; o.x03 = v; o.x04 = v; o.x05 = v; o.x06 = v; o.x07 = v;
  o.x08 = v; o.x09 = v; o.x10 = v; o.x11 = v; o.x12 = v; o.x13 = v; o.x14 = v;
  o.x15 = v; o.x16 = v; o.x17 = v; o.x18 = v; o.x19 = v; o.x20 = v; o.x21 = v;
  o.x22 = v; o.x23 = v; o.x24 = v; o.x25 = v; o.x26 = v; o.x27 = v; o.x28 = v;
  o.x29 = v; o.x30 = v; o.x31 = v; o.x32 = v; o.x33 = v; o.x34 = v; o.x35 = v;
  o.x36 = v; o.x37 = v; o.x38 = v; o.x39 = v; o.x40 = v; o.x41 = v; o.x42 = v;
  o.y01 = v; o.y02 = v; o.y03 = v; o.y04 = v; o.y05 = v; o.y06 = v; o.y07 = v;
  o.y08 = v; o.y09 = v; o.y10 = v; o.y11 = v; o.y12 = v; o.y13 = v; o.y14 = v;
  o.y15 = v; o.y16 = v; o.y17 = v; o.y18 = v; o.y19 = v; o.y20 = v; o.y21 = v;
}

// Check that large object migrations work.
var o4 = {};
// All smi.
set_large(o4, 0);
assertTrue(%HasFastProperties(o4));
// All double.
set_large(o4, 1.5);
// o5 is immediately allocated with doubles.
var o5 = {};
set_large(o5, 0);
assertTrue(%HaveSameMap(o4, o5));

function create_smi_object2() {
  var o = {};
  o.a = 1;
  o.b = 2;
  o.c = 3;
  return o;
}

// All smi
var o6 = create_smi_object2();
var o7 = create_smi_object2();

assertTrue(%HaveSameMap(o6, o7));
// Smi, double, smi.
o6.b = 1.5;
assertFalse(%HaveSameMap(o6, o7));
// Smi, double, object.
o7.c = {};
assertTrue(%HaveSameMap(o6, o7));
// Smi, double, object.
o6.c = {};
assertTrue(%HaveSameMap(o6, o7));

function poly_load(o, b) {
  var v = o.field;
  if (b) {
    return v + 10;
  }
  return o;
}

var of1 = {a:0};
of1.field = {};
var of2 = {b:0};
of2.field = 10;

poly_load(of1, false);
poly_load(of1, false);
poly_load(of2, true);
%OptimizeFunctionOnNextCall(poly_load);
assertEquals("[object Object]10", poly_load(of1, true));

// Ensure small object literals with doubles do not share double storage.
function object_literal() { return {"a":1.5}; }
var o8 = object_literal();
var o9 = object_literal();
o8.a = 4.6
assertEquals(1.5, o9.a);

// Ensure double storage is not leaked in the case of polymorphic loads.
function load_poly(o) {
  return o.a;
}

var o10 = { "a": 1.6 };
var o11 = { "b": 1, "a": 1.7 };
load_poly(o10);
load_poly(o10);
load_poly(o11);
%OptimizeFunctionOnNextCall(load_poly);
var val = load_poly(o10);
o10.a = 19.5;
assertFalse(o10.a == val);

// Ensure polymorphic loads only go monomorphic when the representations are
// compatible.

// Check polymorphic load from double + object fields.
function load_mono(o) {
  return o.a1;
}

var object = {"x": 1};
var o10 = { "a1": 1.6 };
var o11 = { "a1": object, "b": 1 };
load_mono(o10);
load_mono(o10);
load_mono(o11);
%OptimizeFunctionOnNextCall(load_mono);
assertEquals(object, load_mono(o11));

// Check polymorphic load from smi + object fields.
function load_mono2(o) {
  return o.a2;
}

var o12 = { "a2": 5 };
var o13 = { "a2": object, "b": 1 };
load_mono2(o12);
load_mono2(o12);
load_mono2(o13);
%OptimizeFunctionOnNextCall(load_mono2);
assertEquals(object, load_mono2(o13));

// Check polymorphic load from double + double fields.
function load_mono3(o) {
  return o.a3;
}

var o14 = { "a3": 1.6 };
var o15 = { "a3": 1.8, "b": 1 };
load_mono3(o14);
load_mono3(o14);
load_mono3(o15);
%OptimizeFunctionOnNextCall(load_mono3);
assertEquals(1.6, load_mono3(o14));
assertEquals(1.8, load_mono3(o15));

// Check that JSON parsing respects existing representations.
var o16 = JSON.parse('{"a":1.5}');
var o17 = JSON.parse('{"a":100}');
assertTrue(%HaveSameMap(o16, o17));
var o17_a = o17.a;
assertEquals(100, o17_a);
o17.a = 200;
assertEquals(100, o17_a);
assertEquals(200, o17.a);

// Ensure normalizing results in ignored representations.
var o18 = {};
o18.field1 = 100;
o18.field2 = 1;
o18.to_delete = 100;

var o19 = {};
o19.field1 = 100;
o19.field2 = 1.6;
o19.to_delete = 100;

assertFalse(%HaveSameMap(o18, o19));

delete o18.to_delete;
delete o19.to_delete;

assertEquals(1, o18.field2);
assertEquals(1.6, o19.field2);

// Test megamorphic keyed stub behaviour in combination with representations.
var some_object20 = {"a":1};
var o20 = {};
o20.smi = 1;
o20.dbl = 1.5;
o20.obj = some_object20;

function keyed_load(o, k) {
  return o[k];
}

function keyed_store(o, k, v) {
  return o[k] = v;
}

var smi20 = keyed_load(o20, "smi");
var dbl20 = keyed_load(o20, "dbl");
var obj20 = keyed_load(o20, "obj");
keyed_load(o20, "smi");
keyed_load(o20, "dbl");
keyed_load(o20, "obj");
keyed_load(o20, "smi");
keyed_load(o20, "dbl");
keyed_load(o20, "obj");

assertEquals(1, smi20);
assertEquals(1.5, dbl20);
assertEquals(some_object20, obj20);

keyed_store(o20, "smi", 100);
keyed_store(o20, "dbl", 100);
keyed_store(o20, "obj", 100);
keyed_store(o20, "smi", 100);
keyed_store(o20, "dbl", 100);
keyed_store(o20, "obj", 100);
keyed_store(o20, "smi", 100);
keyed_store(o20, "dbl", 100);
keyed_store(o20, "obj", 100);

assertEquals(1, smi20);
assertEquals(1.5, dbl20);
assertEquals(some_object20, obj20);

assertEquals(100, o20.smi);
assertEquals(100, o20.dbl);
assertEquals(100, o20.dbl);

function attr_mismatch_obj(v, writable) {
  var o = {};
  // Assign twice to make the field non-constant.
  // TODO(ishell): update test once constant field tracking is done.
  o.some_value = 0;
  o.some_value = v;
  Object.defineProperty(o, "second_value", {value:10, writable:writable});
  return o;
}

function is_writable(o, p) {
  return Object.getOwnPropertyDescriptor(o,p).writable;
}

var writable = attr_mismatch_obj(10, true);
var non_writable1 = attr_mismatch_obj(10.5, false);
assertTrue(is_writable(writable,"second_value"));
assertFalse(is_writable(non_writable1,"second_value"));
writable.some_value = 20.5;
assertTrue(is_writable(writable,"second_value"));
var non_writable2 = attr_mismatch_obj(10.5, false);
assertTrue(%HaveSameMap(non_writable1, non_writable2));

function test_f(v) {
  var o = {};
  o.vbf = v;
  o.func = test_f;
  return o;
}

function test_fic(o) {
  return o.vbf;
}

var ftest1 = test_f(10);
var ftest2 = test_f(10);
var ftest3 = test_f(10.5);
var ftest4 = test_f(10);
assertFalse(%HaveSameMap(ftest1, ftest3));
assertTrue(%HaveSameMap(ftest3, ftest4));
ftest2.func = is_writable;
test_fic(ftest1);
test_fic(ftest2);
test_fic(ftest3);
test_fic(ftest4);
assertTrue(%HaveSameMap(ftest1, ftest3));
assertTrue(%HaveSameMap(ftest3, ftest4));

// Test representations and transition conversions.
function read_first_double(o) {
  return o.first_double;
}
var df1 = {};
df1.first_double=1.6;
read_first_double(df1);
read_first_double(df1);
function some_function1() { return 10; }
var df2 = {};
df2.first_double = 1.7;
df2.second_function = some_function1;
function some_function2() { return 20; }
var df3 = {};
df3.first_double = 1.7;
df3.second_function = some_function2;
df1.first_double = 10;
read_first_double(df1);

// Test boilerplates with computed values.
function none_boilerplate(a) {
  return {"a_none":a};
}
%OptimizeFunctionOnNextCall(none_boilerplate);
var none_double1 = none_boilerplate(1.7);
var none_double2 = none_boilerplate(1.9);
assertTrue(%HaveSameMap(none_double1, none_double2));
assertEquals(1.7, none_double1.a_none);
assertEquals(1.9, none_double2.a_none);
none_double2.a_none = 3.5;
var none_double1 = none_boilerplate(1.7);
var none_double2 = none_boilerplate(3.5);

function none_to_smi(a) {
  return {"a_smi":a};
}

var none_smi1 = none_to_smi(1);
var none_smi2 = none_to_smi(2);
%OptimizeFunctionOnNextCall(none_to_smi);
var none_smi3 = none_to_smi(3);
assertTrue(%HaveSameMap(none_smi1, none_smi2));
assertTrue(%HaveSameMap(none_smi1, none_smi3));
assertEquals(1, none_smi1.a_smi);
assertEquals(2, none_smi2.a_smi);
assertEquals(3, none_smi3.a_smi);

function none_to_double(a) {
  return {"a_double":a};
}

var none_to_double1 = none_to_double(1.5);
var none_to_double2 = none_to_double(2.8);
%OptimizeFunctionOnNextCall(none_to_double);
var none_to_double3 = none_to_double(3.7);
assertTrue(%HaveSameMap(none_to_double1, none_to_double2));
assertTrue(%HaveSameMap(none_to_double1, none_to_double3));
assertEquals(1.5, none_to_double1.a_double);
assertEquals(2.8, none_to_double2.a_double);
assertEquals(3.7, none_to_double3.a_double);

function none_to_object(a) {
  return {"an_object":a};
}

var none_to_object1 = none_to_object(true);
var none_to_object2 = none_to_object(false);
%OptimizeFunctionOnNextCall(none_to_object);
var none_to_object3 = none_to_object(3.7);
assertTrue(%HaveSameMap(none_to_object1, none_to_object2));
assertTrue(%HaveSameMap(none_to_object1, none_to_object3));
assertEquals(true, none_to_object1.an_object);
assertEquals(false, none_to_object2.an_object);
assertEquals(3.7, none_to_object3.an_object);

function double_to_object(a) {
  var o = {"d_to_h":1.8};
  o.d_to_h = a;
  return o;
}

var dh1 = double_to_object(true);
var dh2 = double_to_object(false);
assertTrue(%HaveSameMap(dh1, dh2));
assertEquals(true, dh1.d_to_h);
assertEquals(false, dh2.d_to_h);

function smi_to_object(a) {
  var o = {"s_to_t":18};
  o.s_to_t = a;
  return o;
}

var st1 = smi_to_object(true);
var st2 = smi_to_object(false);
assertTrue(%HaveSameMap(st1, st2));
assertEquals(true, st1.s_to_t);
assertEquals(false, st2.s_to_t);
