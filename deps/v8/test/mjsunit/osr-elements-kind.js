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

// Flags: --allow-natives-syntax --expose-gc

// Limit the number of stress runs to reduce polymorphism it defeats some of the
// assumptions made about how elements transitions work because transition stubs
// end up going generic.
// Flags: --stress-runs=1

var elements_kind = {
  fast_smi_only             :  'fast smi only elements',
  fast                      :  'fast elements',
  fast_double               :  'fast double elements',
  dictionary                :  'dictionary elements',
  fixed_int32               :  'fixed int8 elements',
  fixed_uint8               :  'fixed uint8 elements',
  fixed_int16               :  'fixed int16 elements',
  fixed_uint16              :  'fixed uint16 elements',
  fixed_int32               :  'fixed int32 elements',
  fixed_uint32              :  'fixed uint32 elements',
  fixed_float32             :  'fixed float32 elements',
  fixed_float64             :  'fixed float64 elements',
  fixed_uint8_clamped       :  'fixed uint8_clamped elements'
}

function getKind(obj) {
  if (%HasSmiElements(obj)) return elements_kind.fast_smi_only;
  if (%HasObjectElements(obj)) return elements_kind.fast;
  if (%HasDoubleElements(obj)) return elements_kind.fast_double;
  if (%HasDictionaryElements(obj)) return elements_kind.dictionary;

  if (%HasFixedInt8Elements(obj)) {
    return elements_kind.fixed_int8;
  }
  if (%HasFixedUint8Elements(obj)) {
    return elements_kind.fixed_uint8;
  }
  if (%HasFixedInt16Elements(obj)) {
    return elements_kind.fixed_int16;
  }
  if (%HasFixedUint16Elements(obj)) {
    return elements_kind.fixed_uint16;
  }
  if (%HasFixedInt32Elements(obj)) {
    return elements_kind.fixed_int32;
  }
  if (%HasFixedUint32Elements(obj)) {
    return elements_kind.fixed_uint32;
  }
  if (%HasFixedFloat32Elements(obj)) {
    return elements_kind.fixed_float32;
  }
  if (%HasFixedFloat64Elements(obj)) {
    return elements_kind.fixed_float64;
  }
  if (%HasFixedUint8ClampedElements(obj)) {
    return elements_kind.fixed_uint8_clamped;
  }
}

function assertKind(expected, obj, name_opt) {
  assertEquals(expected, getKind(obj), name_opt);
}

%NeverOptimizeFunction(construct_smis);
%NeverOptimizeFunction(construct_doubles);
%NeverOptimizeFunction(convert_mixed);
for (var i = 0; i < 10; i++) { if (i == 5) %OptimizeOsr(); }

// This code exists to eliminate the learning influence of AllocationSites
// on the following tests.
var __sequence = 0;
function make_array_string() {
  this.__sequence = this.__sequence + 1;
  return "/* " + this.__sequence + " */  [0, 0, 0];"
}
function make_array() {
  return eval(make_array_string());
}

function construct_smis() {
  var a = make_array();
  a[0] = 0;  // Send the COW array map to the steak house.
  assertKind(elements_kind.fast_smi_only, a);
  return a;
}
function construct_doubles() {
  var a = construct_smis();
  a[0] = 1.5;
  assertKind(elements_kind.fast_double, a);
  return a;
}

// Test transition chain SMI->DOUBLE->FAST (optimized function will
// transition to FAST directly).
function convert_mixed(array, value, kind) {
  array[1] = value;
  assertKind(kind, array);
  assertEquals(value, array[1]);
}
smis = construct_smis();
convert_mixed(smis, 1.5, elements_kind.fast_double);

doubles = construct_doubles();
convert_mixed(doubles, "three", elements_kind.fast);

convert_mixed(construct_smis(), "three", elements_kind.fast);
convert_mixed(construct_doubles(), "three", elements_kind.fast);

if (%ICsAreEnabled()) {
  // Test that allocation sites allocate correct elements kind initially based
  // on previous transitions.
  smis = construct_smis();
  doubles = construct_doubles();
  convert_mixed(smis, 1, elements_kind.fast);
  convert_mixed(doubles, 1, elements_kind.fast);
  assertTrue(%HaveSameMap(smis, doubles));
}

// Throw away type information in the ICs for next stress run.
gc();
