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

// Flags: --allow-natives-syntax --smi-only-arrays --expose-gc
// Flags: --notrack_allocation_sites

// Limit the number of stress runs to reduce polymorphism it defeats some of the
// assumptions made about how elements transitions work because transition stubs
// end up going generic.
// Flags: --stress-runs=2

// Test element kind of objects.
// Since --smi-only-arrays affects builtins, its default setting at compile
// time sticks if built with snapshot.  If --smi-only-arrays is deactivated
// by default, only a no-snapshot build actually has smi-only arrays enabled
// in this test case.  Depending on whether smi-only arrays are actually
// enabled, this test takes the appropriate code path to check smi-only arrays.

// Reset the GC stress mode to be off. Needed because AllocationMementos only
// live for one gc, so a gc that happens in certain fragile areas of the test
// can break assumptions.
%SetFlags("--gc-interval=-1")

support_smi_only_arrays = %HasFastSmiElements(new Array(1,2,3,4,5,6,7,8));

if (support_smi_only_arrays) {
  print("Tests include smi-only arrays.");
} else {
  print("Tests do NOT include smi-only arrays.");
}

var elements_kind = {
  fast_smi_only            :  'fast smi only elements',
  fast                     :  'fast elements',
  fast_double              :  'fast double elements',
  dictionary               :  'dictionary elements',
  external_byte            :  'external byte elements',
  external_unsigned_byte   :  'external unsigned byte elements',
  external_short           :  'external short elements',
  external_unsigned_short  :  'external unsigned short elements',
  external_int             :  'external int elements',
  external_unsigned_int    :  'external unsigned int elements',
  external_float           :  'external float elements',
  external_double          :  'external double elements',
  external_pixel           :  'external pixel elements'
}

function getKind(obj) {
  if (%HasFastSmiElements(obj)) return elements_kind.fast_smi_only;
  if (%HasFastObjectElements(obj)) return elements_kind.fast;
  if (%HasFastDoubleElements(obj)) return elements_kind.fast_double;
  if (%HasDictionaryElements(obj)) return elements_kind.dictionary;
  // Every external kind is also an external array.
  assertTrue(%HasExternalArrayElements(obj));
  if (%HasExternalByteElements(obj)) {
    return elements_kind.external_byte;
  }
  if (%HasExternalUnsignedByteElements(obj)) {
    return elements_kind.external_unsigned_byte;
  }
  if (%HasExternalShortElements(obj)) {
    return elements_kind.external_short;
  }
  if (%HasExternalUnsignedShortElements(obj)) {
    return elements_kind.external_unsigned_short;
  }
  if (%HasExternalIntElements(obj)) {
    return elements_kind.external_int;
  }
  if (%HasExternalUnsignedIntElements(obj)) {
    return elements_kind.external_unsigned_int;
  }
  if (%HasExternalFloatElements(obj)) {
    return elements_kind.external_float;
  }
  if (%HasExternalDoubleElements(obj)) {
    return elements_kind.external_double;
  }
  if (%HasExternalPixelElements(obj)) {
    return elements_kind.external_pixel;
  }
}

function assertKind(expected, obj, name_opt) {
  if (!support_smi_only_arrays &&
      expected == elements_kind.fast_smi_only) {
    expected = elements_kind.fast;
  }
  assertEquals(expected, getKind(obj), name_opt);
}

%NeverOptimizeFunction(construct_smis);
function construct_smis() {
  var a = [0, 0, 0];
  a[0] = 0;  // Send the COW array map to the steak house.
  assertKind(elements_kind.fast_smi_only, a);
  return a;
}

%NeverOptimizeFunction(construct_doubles);
function construct_doubles() {
  var a = construct_smis();
  a[0] = 1.5;
  assertKind(elements_kind.fast_double, a);
  return a;
}

%NeverOptimizeFunction(convert_mixed);
function convert_mixed(array, value, kind) {
  array[1] = value;
  assertKind(kind, array);
  assertEquals(value, array[1]);
}

function test1() {
  if (!support_smi_only_arrays) return;

  // Test transition chain SMI->DOUBLE->FAST (crankshafted function will
  // transition to FAST directly).
  var smis = construct_smis();
  convert_mixed(smis, 1.5, elements_kind.fast_double);

  var doubles = construct_doubles();
  convert_mixed(doubles, "three", elements_kind.fast);

  convert_mixed(construct_smis(), "three", elements_kind.fast);
  convert_mixed(construct_doubles(), "three", elements_kind.fast);

  smis = construct_smis();
  doubles = construct_doubles();
  convert_mixed(smis, 1, elements_kind.fast);
  convert_mixed(doubles, 1, elements_kind.fast);
  assertTrue(%HaveSameMap(smis, doubles));
}

test1();
gc(); // clear IC state
test1();
gc(); // clear IC state
%OptimizeFunctionOnNextCall(test1);
test1();
gc(); // clear IC state
