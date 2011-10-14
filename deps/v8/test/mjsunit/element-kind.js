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

// Flags: --allow-natives-syntax --smi-only-arrays
// Test element kind of objects.
// Since --smi-only-arrays affects builtins, its default setting at compile
// time sticks if built with snapshot.  If --smi-only-arrays is deactivated
// by default, only a no-snapshot build actually has smi-only arrays enabled
// in this test case.  Depending on whether smi-only arrays are actually
// enabled, this test takes the appropriate code path to check smi-only arrays.


support_smi_only_arrays = %HasFastSmiOnlyElements([]);

if (support_smi_only_arrays) {
  print("Tests include smi-only arrays.");
} else {
  print("Tests do NOT include smi-only arrays.");
}

var element_kind = {
  fast_smi_only_elements            :  0,
  fast_elements                     :  1,
  fast_double_elements              :  2,
  dictionary_elements               :  3,
  external_byte_elements            :  4,
  external_unsigned_byte_elements   :  5,
  external_short_elements           :  6,
  external_unsigned_short_elements  :  7,
  external_int_elements             :  8,
  external_unsigned_int_elements    :  9,
  external_float_elements           : 10,
  external_double_elements          : 11,
  external_pixel_elements           : 12
}

// We expect an object to only be of one element kind.
function assertKind(expected, obj) {
  if (support_smi_only_arrays) {
    assertEquals(expected == element_kind.fast_smi_only_elements,
                 %HasFastSmiOnlyElements(obj));
    assertEquals(expected == element_kind.fast_elements,
                 %HasFastElements(obj));
  } else {
    assertEquals(expected == element_kind.fast_elements ||
                 expected == element_kind.fast_smi_only_elements,
                 %HasFastElements(obj));
  }
  assertEquals(expected == element_kind.fast_double_elements,
               %HasFastDoubleElements(obj));
  assertEquals(expected == element_kind.dictionary_elements,
               %HasDictionaryElements(obj));
  assertEquals(expected == element_kind.external_byte_elements,
               %HasExternalByteElements(obj));
  assertEquals(expected == element_kind.external_unsigned_byte_elements,
               %HasExternalUnsignedByteElements(obj));
  assertEquals(expected == element_kind.external_short_elements,
               %HasExternalShortElements(obj));
  assertEquals(expected == element_kind.external_unsigned_short_elements,
               %HasExternalUnsignedShortElements(obj));
  assertEquals(expected == element_kind.external_int_elements,
               %HasExternalIntElements(obj));
  assertEquals(expected == element_kind.external_unsigned_int_elements,
               %HasExternalUnsignedIntElements(obj));
  assertEquals(expected == element_kind.external_float_elements,
               %HasExternalFloatElements(obj));
  assertEquals(expected == element_kind.external_double_elements,
               %HasExternalDoubleElements(obj));
  assertEquals(expected == element_kind.external_pixel_elements,
               %HasExternalPixelElements(obj));
  // every external kind is also an external array
  assertEquals(expected >= element_kind.external_byte_elements,
               %HasExternalArrayElements(obj));
}

var me = {};
assertKind(element_kind.fast_elements, me);
me.dance = 0xD15C0;
me.drink = 0xC0C0A;
assertKind(element_kind.fast_elements, me);

var too = [1,2,3];
assertKind(element_kind.fast_smi_only_elements, too);
too.dance = 0xD15C0;
too.drink = 0xC0C0A;
assertKind(element_kind.fast_smi_only_elements, too);

// Make sure the element kind transitions from smionly when a non-smi is stored.
var you = new Array();
assertKind(element_kind.fast_smi_only_elements, you);
for (var i = 0; i < 1337; i++) {
  var val = i;
  if (i == 1336) {
    assertKind(element_kind.fast_smi_only_elements, you);
    val = new Object();
  }
  you[i] = val;
}
assertKind(element_kind.fast_elements, you);

assertKind(element_kind.dictionary_elements, new Array(0xDECAF));

var fast_double_array = new Array(0xDECAF);
for (var i = 0; i < 0xDECAF; i++) fast_double_array[i] = i / 2;
assertKind(element_kind.fast_double_elements, fast_double_array);

assertKind(element_kind.external_byte_elements,           new Int8Array(9001));
assertKind(element_kind.external_unsigned_byte_elements,  new Uint8Array(007));
assertKind(element_kind.external_short_elements,          new Int16Array(666));
assertKind(element_kind.external_unsigned_short_elements, new Uint16Array(42));
assertKind(element_kind.external_int_elements,            new Int32Array(0xF));
assertKind(element_kind.external_unsigned_int_elements,   new Uint32Array(23));
assertKind(element_kind.external_float_elements,          new Float32Array(7));
assertKind(element_kind.external_double_elements,         new Float64Array(0));
assertKind(element_kind.external_pixel_elements,          new PixelArray(512));

// Crankshaft support for smi-only array elements.
function monomorphic(array) {
  for (var i = 0; i < 3; i++) {
    array[i] = i + 10;
  }
  assertKind(element_kind.fast_smi_only_elements, array);
  for (var i = 0; i < 3; i++) {
    var a = array[i];
    assertEquals(i + 10, a);
  }
}
var smi_only = [1, 2, 3];
for (var i = 0; i < 3; i++) monomorphic(smi_only);
%OptimizeFunctionOnNextCall(monomorphic);
monomorphic(smi_only);
function polymorphic(array, expected_kind) {
  array[1] = 42;
  assertKind(expected_kind, array);
  var a = array[1];
  assertEquals(42, a);
}
var smis = [1, 2, 3];
var strings = ["one", "two", "three"];
var doubles = [0, 0, 0]; doubles[0] = 1.5; doubles[1] = 2.5; doubles[2] = 3.5;
assertKind(support_smi_only_arrays
               ? element_kind.fast_double_elements
               : element_kind.fast_elements,
           doubles);
for (var i = 0; i < 3; i++) {
  polymorphic(smis, element_kind.fast_smi_only_elements);
  polymorphic(strings, element_kind.fast_elements);
  polymorphic(doubles, support_smi_only_arrays
                           ? element_kind.fast_double_elements
                           : element_kind.fast_elements);
}
%OptimizeFunctionOnNextCall(polymorphic);
polymorphic(smis, element_kind.fast_smi_only_elements);
polymorphic(strings, element_kind.fast_elements);
polymorphic(doubles, support_smi_only_arrays
    ? element_kind.fast_double_elements
    : element_kind.fast_elements);

// Crankshaft support for smi-only elements in dynamic array literals.
function get(foo) { return foo; }  // Used to generate dynamic values.

function crankshaft_test() {
  var a = [get(1), get(2), get(3)];
  assertKind(element_kind.fast_smi_only_elements, a);
  var b = [get(1), get(2), get("three")];
  assertKind(element_kind.fast_elements, b);
  var c = [get(1), get(2), get(3.5)];
  // The full code generator doesn't support conversion to fast_double_elements
  // yet. Crankshaft does, but only with --smi-only-arrays support.
  if ((%GetOptimizationStatus(crankshaft_test) & 1) &&
      support_smi_only_arrays) {
    assertKind(element_kind.fast_double_elements, c);
  } else {
    assertKind(element_kind.fast_elements, c);
  }
}
for (var i = 0; i < 3; i++) {
  crankshaft_test();
}
%OptimizeFunctionOnNextCall(crankshaft_test);
crankshaft_test();

// Elements_kind transitions for arrays.

// A map can have three different elements_kind transitions: SMI->DOUBLE,
// DOUBLE->OBJECT, and SMI->OBJECT. No matter in which order these three are
// created, they must always end up with the same FAST map.

// This test is meaningless without FAST_SMI_ONLY_ELEMENTS.
if (support_smi_only_arrays) {
  // Preparation: create one pair of identical objects for each case.
  var a = [1, 2, 3];
  var b = [1, 2, 3];
  assertTrue(%HaveSameMap(a, b));
  assertKind(element_kind.fast_smi_only_elements, a);
  var c = [1, 2, 3];
  c["case2"] = true;
  var d = [1, 2, 3];
  d["case2"] = true;
  assertTrue(%HaveSameMap(c, d));
  assertFalse(%HaveSameMap(a, c));
  assertKind(element_kind.fast_smi_only_elements, c);
  var e = [1, 2, 3];
  e["case3"] = true;
  var f = [1, 2, 3];
  f["case3"] = true;
  assertTrue(%HaveSameMap(e, f));
  assertFalse(%HaveSameMap(a, e));
  assertFalse(%HaveSameMap(c, e));
  assertKind(element_kind.fast_smi_only_elements, e);
  // Case 1: SMI->DOUBLE, DOUBLE->OBJECT, SMI->OBJECT.
  a[0] = 1.5;
  assertKind(element_kind.fast_double_elements, a);
  a[0] = "foo";
  assertKind(element_kind.fast_elements, a);
  b[0] = "bar";
  assertTrue(%HaveSameMap(a, b));
  // Case 2: SMI->DOUBLE, SMI->OBJECT, DOUBLE->OBJECT.
  c[0] = 1.5;
  assertKind(element_kind.fast_double_elements, c);
  assertFalse(%HaveSameMap(c, d));
  d[0] = "foo";
  assertKind(element_kind.fast_elements, d);
  assertFalse(%HaveSameMap(c, d));
  c[0] = "bar";
  assertTrue(%HaveSameMap(c, d));
  // Case 3: SMI->OBJECT, SMI->DOUBLE, DOUBLE->OBJECT.
  e[0] = "foo";
  assertKind(element_kind.fast_elements, e);
  assertFalse(%HaveSameMap(e, f));
  f[0] = 1.5;
  assertKind(element_kind.fast_double_elements, f);
  assertFalse(%HaveSameMap(e, f));
  f[0] = "bar";
  assertKind(element_kind.fast_elements, f);
  assertTrue(%HaveSameMap(e, f));
}
