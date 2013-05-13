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
assertFalse(%HaveSameMap(o6, o7));
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

assertTrue(%HaveSameMap(o18, o19));
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
