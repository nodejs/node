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
// Flags: --noalways-opt

// Test element kind of objects.
// Since --smi-only-arrays affects builtins, its default setting at compile
// time sticks if built with snapshot.  If --smi-only-arrays is deactivated
// by default, only a no-snapshot build actually has smi-only arrays enabled
// in this test case.  Depending on whether smi-only arrays are actually
// enabled, this test takes the appropriate code path to check smi-only arrays.

// support_smi_only_arrays = %HasFastSmiElements(new Array(1,2,3,4,5,6,7,8));
support_smi_only_arrays = true;

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
  assertKind(elements_kind.fast_double, obj);

  // The test below is in a loop because arrays that live
  // at global scope without the chance of being recreated
  // don't have allocation site information attached.
  for (i = 0; i < 2; i++) {
    obj = fastliteralcase([5, 3, 2], 1.5);
    assertKind(elements_kind.fast_double, obj);
    obj = fastliteralcase([3, 6, 2], 1.5);
    assertKind(elements_kind.fast_double, obj);

    // Note: thanks to pessimistic transition store stubs, we'll attempt
    // to transition to the most general elements kind seen at a particular
    // store site. So, the elements kind will be double.
    obj = fastliteralcase([2, 6, 3], 2);
    assertKind(elements_kind.fast_double, obj);
  }

  // Verify that we will not pretransition the double->fast path.
  obj = fastliteralcase(get_standard_literal(), "elliot");
  assertKind(elements_kind.fast, obj);
  obj = fastliteralcase(get_standard_literal(), 3);
  assertKind(elements_kind.fast, obj);

  // Make sure this works in crankshafted code too.
  %OptimizeFunctionOnNextCall(get_standard_literal);
  get_standard_literal();
  obj = get_standard_literal();
  assertKind(elements_kind.fast, obj);

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
  assertKind(elements_kind.fast, obj);

  // Case: make sure transitions from packed to holey are tracked
  function fastliteralcase_smiholey(index, value) {
    var literal = [1, 2, 3, 4];
    literal[index] = value;
    return literal;
  }

  obj = fastliteralcase_smiholey(5, 1);
  assertKind(elements_kind.fast_smi_only, obj);
  assertHoley(obj);
  obj = fastliteralcase_smiholey(0, 1);
  assertKind(elements_kind.fast_smi_only, obj);
  assertHoley(obj);

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

  // Try to continue the transition to fast object.
  // TODO(mvstanton): re-enable commented out code when
  // FLAG_pretenuring_call_new is turned on in the build.
  obj = newarraycase_length_smidouble("coates");
  assertKind(elements_kind.fast, obj);
  obj = newarraycase_length_smidouble(2);
  // assertKind(elements_kind.fast, obj);

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

  // Case: array constructor calls with out of date feedback.
  // The boilerplate should incorporate all feedback, but the input array
  // should be minimally transitioned based on immediate need.
  (function() {
    function foo(i) {
      // We have two cases, one for literals one for constructed arrays.
      var a = (i == 0)
        ? [1, 2, 3]
        : new Array(1, 2, 3);
      return a;
    }

    for (i = 0; i < 2; i++) {
      a = foo(i);
      b = foo(i);
      b[5] = 1;  // boilerplate goes holey
      assertHoley(foo(i));
      a[0] = 3.5;  // boilerplate goes holey double
      assertKind(elements_kind.fast_double, a);
      assertNotHoley(a);
      c = foo(i);
      assertKind(elements_kind.fast_double, c);
      assertHoley(c);
    }
  })();

  function newarraycase_onearg(len, value) {
    var a = new Array(len);
    a[0] = value;
    return a;
  }

  obj = newarraycase_onearg(5, 3.5);
  assertKind(elements_kind.fast_double, obj);
  obj = newarraycase_onearg(10, 5);
  assertKind(elements_kind.fast_double, obj);
  obj = newarraycase_onearg(0, 5);
  assertKind(elements_kind.fast_double, obj);
  // Now pass a length that forces the dictionary path.
  obj = newarraycase_onearg(100000, 5);
  assertKind(elements_kind.dictionary, obj);
  assertTrue(obj.length == 100000);

  // Verify that cross context calls work
  var realmA = Realm.current();
  var realmB = Realm.create();
  assertEquals(0, realmA);
  assertEquals(1, realmB);

  function instanceof_check(type) {
    assertTrue(new type() instanceof type);
    assertTrue(new type(5) instanceof type);
    assertTrue(new type(1,2,3) instanceof type);
  }

  function instanceof_check2(type) {
    assertTrue(new type() instanceof type);
    assertTrue(new type(5) instanceof type);
    assertTrue(new type(1,2,3) instanceof type);
  }

  var realmBArray = Realm.eval(realmB, "Array");
  instanceof_check(Array);
  instanceof_check(realmBArray);

  // instanceof_check2 is here because the call site goes through a state.
  // Since instanceof_check(Array) was first called with the current context
  // Array function, it went from (uninit->Array) then (Array->megamorphic).
  // We'll get a different state traversal if we start with realmBArray.
  // It'll go (uninit->realmBArray) then (realmBArray->megamorphic). Recognize
  // that state "Array" implies an AllocationSite is present, and code is
  // configured to use it.
  instanceof_check2(realmBArray);
  instanceof_check2(Array);

  %OptimizeFunctionOnNextCall(instanceof_check);

  // No de-opt will occur because HCallNewArray wasn't selected, on account of
  // the call site not being monomorphic to Array.
  instanceof_check(Array);
  assertOptimized(instanceof_check);
  instanceof_check(realmBArray);
  assertOptimized(instanceof_check);

  // Try to optimize again, but first clear all type feedback, and allow it
  // to be monomorphic on first call. Only after crankshafting do we introduce
  // realmBArray. This should deopt the method.
  %DeoptimizeFunction(instanceof_check);
  %ClearFunctionTypeFeedback(instanceof_check);
  instanceof_check(Array);
  instanceof_check(Array);
  %OptimizeFunctionOnNextCall(instanceof_check);
  instanceof_check(Array);
  assertOptimized(instanceof_check);

  instanceof_check(realmBArray);
  assertUnoptimized(instanceof_check);

  // Case: make sure nested arrays benefit from allocation site feedback as
  // well.
  (function() {
    // Make sure we handle nested arrays
   function get_nested_literal() {
     var literal = [[1,2,3,4], [2], [3]];
     return literal;
   }

   obj = get_nested_literal();
   assertKind(elements_kind.fast, obj);
   obj[0][0] = 3.5;
   obj[2][0] = "hello";
   obj = get_nested_literal();
   assertKind(elements_kind.fast_double, obj[0]);
   assertKind(elements_kind.fast_smi_only, obj[1]);
   assertKind(elements_kind.fast, obj[2]);

   // A more complex nested literal case.
   function get_deep_nested_literal() {
     var literal = [[1], [[2], "hello"], 3, [4]];
     return literal;
   }

   obj = get_deep_nested_literal();
   assertKind(elements_kind.fast_smi_only, obj[1][0]);
   obj[0][0] = 3.5;
   obj[1][0][0] = "goodbye";
   assertKind(elements_kind.fast_double, obj[0]);
   assertKind(elements_kind.fast, obj[1][0]);

   obj = get_deep_nested_literal();
   assertKind(elements_kind.fast_double, obj[0]);
   assertKind(elements_kind.fast, obj[1][0]);
  })();


  // Make sure object literals with array fields benefit from the type feedback
  // that allocation mementos provide.
  (function() {
    // A literal in an object
    function get_object_literal() {
      var literal = {
        array: [1,2,3],
        data: 3.5
      };
      return literal;
    }

    obj = get_object_literal();
    assertKind(elements_kind.fast_smi_only, obj.array);
    obj.array[1] = 3.5;
    assertKind(elements_kind.fast_double, obj.array);
    obj = get_object_literal();
    assertKind(elements_kind.fast_double, obj.array);

    function get_nested_object_literal() {
      var literal = {
        array: [[1],[2],[3]],
        data: 3.5
      };
      return literal;
    }

    obj = get_nested_object_literal();
    assertKind(elements_kind.fast, obj.array);
    assertKind(elements_kind.fast_smi_only, obj.array[1]);
    obj.array[1][0] = 3.5;
    assertKind(elements_kind.fast_double, obj.array[1]);
    obj = get_nested_object_literal();
    assertKind(elements_kind.fast_double, obj.array[1]);

    %OptimizeFunctionOnNextCall(get_nested_object_literal);
    get_nested_object_literal();
    obj = get_nested_object_literal();
    assertKind(elements_kind.fast_double, obj.array[1]);

    // Make sure we handle nested arrays
    function get_nested_literal() {
      var literal = [[1,2,3,4], [2], [3]];
      return literal;
    }

    obj = get_nested_literal();
    assertKind(elements_kind.fast, obj);
    obj[0][0] = 3.5;
    obj[2][0] = "hello";
    obj = get_nested_literal();
    assertKind(elements_kind.fast_double, obj[0]);
    assertKind(elements_kind.fast_smi_only, obj[1]);
    assertKind(elements_kind.fast, obj[2]);

    // A more complex nested literal case.
    function get_deep_nested_literal() {
      var literal = [[1], [[2], "hello"], 3, [4]];
      return literal;
    }

    obj = get_deep_nested_literal();
    assertKind(elements_kind.fast_smi_only, obj[1][0]);
    obj[0][0] = 3.5;
    obj[1][0][0] = "goodbye";
    assertKind(elements_kind.fast_double, obj[0]);
    assertKind(elements_kind.fast, obj[1][0]);

    obj = get_deep_nested_literal();
    assertKind(elements_kind.fast_double, obj[0]);
    assertKind(elements_kind.fast, obj[1][0]);
  })();
}
