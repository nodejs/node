// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Ensure `Array.prototype.fill` functions correctly for numerous elements
// kinds.

// If no arguments are provided, call Array.p.fill without any arguments,
// otherwise the test is allowed to specify what value to use to better control
// ElementsKind transitions. From and to is provided by the harness.
function callAndAssertFill(object, test_value, harness_value, from, to) {
  let value = arguments.length > 2 ? test_value : harness_value;

  Array.prototype.fill.call(object, value, from, to);

  %HeapObjectVerify(object);
  assertArrayHasValueInRange(object, value, from, to);
}

function assertArrayHasValueInRange(obj, value, from, to) {
  for (let i = from; i < to; ++i) {
    assertEquals(value, obj[i]);
  }
}

// Tests are executed multiple times. Creating arrays using literal notation
// will create COW-Arrays, which will propagate the most general ElementsKind
// back to their allocation site.
// pristineArray will always return a 🐄-Array with the ElementsKind we actually
// want.
let i = 0;
function pristineArray(str) {
  return eval(str + "//" + (i++));
}

let tests = {
  ARRAY_PACKED_ELEMENTS(value, from, to) {
    let array = pristineArray(
        `["Some string", {}, /foobar/, "Another string", {}]`);
    assertTrue(%HasObjectElements(array));
    assertFalse(%HasHoleyElements(array));

    callAndAssertFill(array, "42", ...arguments);
  },

  ARRAY_HOLEY_ELEMENTS(value, from, to) {
    let array = pristineArray(`["Some string", , {}, , "Another string"]`);
    assertTrue(%HasObjectElements(array));
    assertTrue(%HasHoleyElements(array));

    callAndAssertFill(array, "42", ...arguments);
  },

  ARRAY_PACKED_SMI_ELEMENTS(value, from, to) {
    let array = pristineArray(`[0, -42, 5555, 23, 6]`);
    assertTrue(%HasSmiElements(array));
    assertFalse(%HasHoleyElements(array));

    callAndAssertFill(array, 42, ...arguments);
  },

  ARRAY_HOLEY_SMI_ELEMENTS(value, from, to) {
    let array = pristineArray(`[0, , 5555, , 6]`);
    assertTrue(%HasSmiElements(array));
    assertTrue(%HasHoleyElements(array));

    callAndAssertFill(array, 42, ...arguments);
  },

  ARRAY_PACKED_DOUBLE_ELEMENTS(value, from, to) {
    let array = pristineArray(`[3.14, 7.00001, NaN, -25.3333, 1.0]`);
    assertTrue(%HasDoubleElements(array));
    assertFalse(%HasHoleyElements(array));

    callAndAssertFill(array, 42.42, ...arguments);
  },

  ARRAY_HOLEY_DOUBLE_ELEMENTS(value, from, to) {
    let array = pristineArray(`[3.14, , , , 1.0]`);
    assertTrue(%HasDoubleElements(array));
    assertTrue(%HasHoleyElements(array));

    callAndAssertFill(array, 42.42, ...arguments);
  },

  ARRAY_DICTIONARY_ELEMENTS(value, from, to) {
    let array = pristineArray(`[0, , 2, 3, 4]`);
    Object.defineProperty(array, 1, { get() { return this.foo; },
                                      set(val) { this.foo = val; }});
    assertTrue(%HasDictionaryElements(array));

    callAndAssertFill(array, "42", ...arguments);
  }

  // TODO(szuend): Add additional tests receivers other than arrays
  //               (Objects, TypedArrays, etc.).
};

function RunTest(test) {
  test();
  test(undefined);
  test(undefined, 1);
  test(undefined, 1, 4);
}

function RunTests(tests) {
  Object.keys(tests).forEach(test => RunTest(tests[test]));
}

RunTests(tests);

Array.prototype.__proto__ = {
  __proto__: Array.prototype.__proto__
};

RunTests(tests);
