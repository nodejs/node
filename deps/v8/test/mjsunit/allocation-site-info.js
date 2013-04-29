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

// Flags: --allow-natives-syntax --smi-only-arrays --expose-gc
// Flags: --track-allocation-sites --noalways-opt

// TODO(mvstanton): remove --nooptimize-constructed-arrays and enable
// the constructed array code below when the feature is turned on
// by default.

// Test element kind of objects.
// Since --smi-only-arrays affects builtins, its default setting at compile
// time sticks if built with snapshot.  If --smi-only-arrays is deactivated
// by default, only a no-snapshot build actually has smi-only arrays enabled
// in this test case.  Depending on whether smi-only arrays are actually
// enabled, this test takes the appropriate code path to check smi-only arrays.

// support_smi_only_arrays = %HasFastSmiElements(new Array(1,2,3,4,5,6,7,8));
support_smi_only_arrays = true;
optimize_constructed_arrays = false;

if (support_smi_only_arrays) {
  print("Tests include smi-only arrays.");
} else {
  print("Tests do NOT include smi-only arrays.");
}

if (optimize_constructed_arrays) {
  print("Tests include constructed array optimizations.");
} else {
  print("Tests do NOT include constructed array optimizations.");
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
}

function isHoley(obj) {
  if (%HasFastHoleyElements(obj)) return true;
  return false;
}

function assertKind(expected, obj, name_opt) {
  if (!support_smi_only_arrays &&
      expected == elements_kind.fast_smi_only) {
    expected = elements_kind.fast;
  }
  assertEquals(expected, getKind(obj), name_opt);
}

function assertHoley(obj, name_opt) {
  assertEquals(true, isHoley(obj), name_opt);
}

function assertNotHoley(obj, name_opt) {
  assertEquals(false, isHoley(obj), name_opt);
}

if (support_smi_only_arrays) {

  obj = [];
  assertNotHoley(obj);
  assertKind(elements_kind.fast_smi_only, obj);

  obj = [1, 2, 3];
  assertNotHoley(obj);
  assertKind(elements_kind.fast_smi_only, obj);

  obj = new Array();
  assertNotHoley(obj);
  assertKind(elements_kind.fast_smi_only, obj);

  obj = new Array(0);
  assertNotHoley(obj);
  assertKind(elements_kind.fast_smi_only, obj);

  obj = new Array(2);
  assertHoley(obj);
  assertKind(elements_kind.fast_smi_only, obj);

  obj = new Array(1,2,3);
  assertNotHoley(obj);
  assertKind(elements_kind.fast_smi_only, obj);

  obj = new Array(1, "hi", 2, undefined);
  assertNotHoley(obj);
  assertKind(elements_kind.fast, obj);

  function fastliteralcase(literal, value) {
    literal[0] = value;
    return literal;
  }

  function get_standard_literal() {
    var literal = [1, 2, 3];
    return literal;
  }

  // Case: [1,2,3] as allocation site
  obj = fastliteralcase(get_standard_literal(), 1);
  assertKind(elements_kind.fast_smi_only, obj);
  obj = fastliteralcase(get_standard_literal(), 1.5);
  assertKind(elements_kind.fast_double, obj);
  obj = fastliteralcase(get_standard_literal(), 2);
  // TODO(hpayer): bring the following assert back as soon as allocation
  // sites work again for fast literals
  //assertKind(elements_kind.fast_double, obj);

  // The test below is in a loop because arrays that live
  // at global scope without the chance of being recreated
  // don't have allocation site information attached.
  for (i = 0; i < 2; i++) {
    obj = fastliteralcase([5, 3, 2], 1.5);
    assertKind(elements_kind.fast_double, obj);
    obj = fastliteralcase([3, 6, 2], 1.5);
    assertKind(elements_kind.fast_double, obj);
    obj = fastliteralcase([2, 6, 3], 2);
    assertKind(elements_kind.fast_smi_only, obj);
  }

  // Verify that we will not pretransition the double->fast path.
  obj = fastliteralcase(get_standard_literal(), "elliot");
  assertKind(elements_kind.fast, obj);
  // This fails until we turn off optimistic transitions to the
  // most general elements kind seen on keyed stores. It's a goal
  // to turn it off, but for now we need it.
  // obj = fastliteralcase(3);
  // assertKind(elements_kind.fast_double, obj);

  // Make sure this works in crankshafted code too.
  %OptimizeFunctionOnNextCall(get_standard_literal);
  get_standard_literal();
  obj = get_standard_literal();
  assertKind(elements_kind.fast_double, obj);

  function fastliteralcase_smifast(value) {
    var literal = [1, 2, 3, 4];
    literal[0] = value;
    return literal;
  }

  obj = fastliteralcase_smifast(1);
  assertKind(elements_kind.fast_smi_only, obj);
  obj = fastliteralcase_smifast("carter");
  assertKind(elements_kind.fast, obj);
  obj = fastliteralcase_smifast(2);
  // TODO(hpayer): bring the following assert back as soon as allocation
  // sites work again for fast literals
  //assertKind(elements_kind.fast, obj);

  if (optimize_constructed_arrays) {
    function newarraycase_smidouble(value) {
      var a = new Array();
      a[0] = value;
      return a;
    }

    // Case: new Array() as allocation site, smi->double
    obj = newarraycase_smidouble(1);
    assertKind(elements_kind.fast_smi_only, obj);
    obj = newarraycase_smidouble(1.5);
    assertKind(elements_kind.fast_double, obj);
    obj = newarraycase_smidouble(2);
    assertKind(elements_kind.fast_double, obj);

    function newarraycase_smiobj(value) {
      var a = new Array();
      a[0] = value;
      return a;
    }

    // Case: new Array() as allocation site, smi->fast
    obj = newarraycase_smiobj(1);
    assertKind(elements_kind.fast_smi_only, obj);
    obj = newarraycase_smiobj("gloria");
    assertKind(elements_kind.fast, obj);
    obj = newarraycase_smiobj(2);
    assertKind(elements_kind.fast, obj);

    function newarraycase_length_smidouble(value) {
      var a = new Array(3);
      a[0] = value;
      return a;
    }

    // Case: new Array(length) as allocation site
    obj = newarraycase_length_smidouble(1);
    assertKind(elements_kind.fast_smi_only, obj);
    obj = newarraycase_length_smidouble(1.5);
    assertKind(elements_kind.fast_double, obj);
    obj = newarraycase_length_smidouble(2);
    assertKind(elements_kind.fast_double, obj);

    // Try to continue the transition to fast object, but
    // we will not pretransition from double->fast, because
    // it may hurt performance ("poisoning").
    obj = newarraycase_length_smidouble("coates");
    assertKind(elements_kind.fast, obj);
    obj = newarraycase_length_smidouble(2.5);
    // However, because of optimistic transitions, we will
    // transition to the most general kind of elements kind found,
    // therefore I can't count on this assert yet.
    // assertKind(elements_kind.fast_double, obj);

    function newarraycase_length_smiobj(value) {
      var a = new Array(3);
      a[0] = value;
      return a;
    }

    // Case: new Array(<length>) as allocation site, smi->fast
    obj = newarraycase_length_smiobj(1);
    assertKind(elements_kind.fast_smi_only, obj);
    obj = newarraycase_length_smiobj("gloria");
    assertKind(elements_kind.fast, obj);
    obj = newarraycase_length_smiobj(2);
    assertKind(elements_kind.fast, obj);

    function newarraycase_list_smidouble(value) {
      var a = new Array(1, 2, 3);
      a[0] = value;
      return a;
    }

    obj = newarraycase_list_smidouble(1);
    assertKind(elements_kind.fast_smi_only, obj);
    obj = newarraycase_list_smidouble(1.5);
    assertKind(elements_kind.fast_double, obj);
    obj = newarraycase_list_smidouble(2);
    assertKind(elements_kind.fast_double, obj);

    function newarraycase_list_smiobj(value) {
      var a = new Array(4, 5, 6);
      a[0] = value;
      return a;
    }

    obj = newarraycase_list_smiobj(1);
    assertKind(elements_kind.fast_smi_only, obj);
    obj = newarraycase_list_smiobj("coates");
    assertKind(elements_kind.fast, obj);
    obj = newarraycase_list_smiobj(2);
    assertKind(elements_kind.fast, obj);
  }
}
