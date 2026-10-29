// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var get_count = 0;
var has_count = 0;
var property_descriptor_count = 0;
globalThis.__proto__ = new Proxy({},
                                 {get() {get_count++},
                                  has() {has_count++;},
                                  getOwnPropertyDescriptor() {property_desciptor_count++}});
function checkCounts(count) {
  assertEquals(has_count, count);
  assertEquals(get_count, 0);
  assertEquals(property_descriptor_count, 0);
}

function load_lookup_global_error() {
  eval("var b = 10");
  x;
}
assertThrows(load_lookup_global_error, ReferenceError);
checkCounts(1);

%EnsureFeedbackVectorForFunction(load_lookup_global_error);
assertThrows(load_lookup_global_error, ReferenceError);
checkCounts(2);
assertThrows(load_lookup_global_error, ReferenceError);
checkCounts(3);

function load_global_error() {
  x;
}
assertThrows(load_global_error, ReferenceError);
checkCounts(4);

%EnsureFeedbackVectorForFunction(load_global_error);
assertThrows(load_global_error, ReferenceError);
checkCounts(5);
assertThrows(load_global_error, ReferenceError);
checkCounts(6);


// Check when the object is present on the proxy.
get_count = 0;
has_count = 0;
property_descriptor_count = 0;

globalThis.__proto__ = new Proxy({},
                                 {get() {get_count++; return 10;},
                                  has() {has_count++; return true;},
                                  getOwnPropertyDescriptor() {property_desciptor_count++}});
function checkCountsWithGet(count) {
  assertEquals(has_count, count);
  assertEquals(get_count, count);
  assertEquals(property_descriptor_count, 0);
}

function load_lookup_global() {
  eval("var b = 10");
  return x;
}
assertEquals(load_lookup_global(), 10);
checkCountsWithGet(1);

%EnsureFeedbackVectorForFunction(load_lookup_global);
assertEquals(load_lookup_global(), 10);
checkCountsWithGet(2);
assertEquals(load_lookup_global(), 10);
checkCountsWithGet(3);

function load_global() {
  return x;
}
assertEquals(load_global(), 10);
checkCountsWithGet(4);

%EnsureFeedbackVectorForFunction(load_global);
assertEquals(load_global(), 10);
checkCountsWithGet(5);
assertEquals(load_global(), 10);
checkCountsWithGet(6);

// Check unbound variable access inside typeof
get_count = 0;
has_count = 0;
property_descriptor_count = 0;

globalThis.__proto__ = new Proxy({},
                                 {get() {get_count++},
                                  has() {has_count++;},
                                  getOwnPropertyDescriptor() {property_desciptor_count++}});
function checkCountsInsideTypeof(count) {
  assertEquals(has_count, count);
  assertEquals(get_count, 0);
  assertEquals(property_descriptor_count, 0);
}

function load_lookup_inside_typeof() {
  eval("var b = 10");
  return typeof(x);
}
assertEquals(load_lookup_inside_typeof(), "undefined");
checkCountsInsideTypeof(1);

%EnsureFeedbackVectorForFunction(load_lookup_inside_typeof);
assertEquals(load_lookup_inside_typeof(), "undefined");
checkCountsInsideTypeof(2);
assertEquals(load_lookup_inside_typeof(), "undefined");
checkCountsInsideTypeof(3);

function load_inside_typeof() {
  return typeof(x);
}
assertEquals(load_inside_typeof(), "undefined");
checkCountsInsideTypeof(4);

%EnsureFeedbackVectorForFunction(load_inside_typeof);
assertEquals(load_inside_typeof(), "undefined");
checkCountsInsideTypeof(5);
assertEquals(load_inside_typeof(), "undefined");
checkCountsInsideTypeof(6);

// Check bound variable access inside typeof
get_count = 0;
has_count = 0;
property_descriptor_count = 0;

globalThis.__proto__ = new Proxy({},
                                 {get() {get_count++; return 10;},
                                  has() {has_count++; return true;},
                                  getOwnPropertyDescriptor() {property_desciptor_count++}});

function checkCountsBoundVarInsideTypeof(count) {
  assertEquals(has_count, count);
  assertEquals(get_count, count);
  assertEquals(property_descriptor_count, 0);
}

function load_lookup_number_inside_typeof() {
  eval("var b = 10");
  return typeof(x);
}
assertEquals(load_lookup_number_inside_typeof(), "number");
checkCountsBoundVarInsideTypeof(1);

%EnsureFeedbackVectorForFunction(load_lookup_number_inside_typeof);
assertEquals(load_lookup_number_inside_typeof(), "number");
checkCountsBoundVarInsideTypeof(2);
assertEquals(load_lookup_number_inside_typeof(), "number");
checkCountsBoundVarInsideTypeof(3);

function load_number_inside_typeof() {
  return typeof(x);
}
assertEquals(load_number_inside_typeof(), "number");
checkCountsBoundVarInsideTypeof(4);

%EnsureFeedbackVectorForFunction(load_inside_typeof);
assertEquals(load_number_inside_typeof(), "number");
checkCountsBoundVarInsideTypeof(5);
assertEquals(load_number_inside_typeof(), "number");
checkCountsBoundVarInsideTypeof(6);

// Check that if has property returns true we don't throw even when get property
// says otherwise.

globalThis.__proto__ = new Proxy({},
                                 {has() {has_count++; return true;},
                                  getOwnPropertyDescriptor() {property_desciptor_count++}});

function load_lookup_global_has_property() {
  eval("var b = 10");
  return x;
}
assertEquals(load_lookup_global_has_property(), undefined);

function load_global_has_property() {
  return x;
}
assertEquals(load_global_has_property(), undefined);
