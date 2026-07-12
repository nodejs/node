// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

(function doubleToTaggedWithTaggedValueStoresCorrectly() {

  function setX_Double(o) { o.x = 4.2; }

  function foo() {
    // o.x starts off as Double
    const o = { x: 0.1 };

    // Write to it a few times with setX_Double, to make sure setX_Double has
    // Double feedback.
    setX_Double(o);
    setX_Double(o);

    // Transition o.x to Tagged.
    o.x = {};

    // setX_Double will still have Double feedback, so make sure it works with
    // the new Tagged representation o.x.
    setX_Double(o);

    assertEquals(o.x, 4.2);
  }

  %EnsureFeedbackVectorForFunction(setX_Double);
  foo();

})();

(function doubleToTaggedWithDoubleValueDoesNotMutate() {

  function setX_Double(o) { o.x = 4.2; }

  function foo() {
    // o.x starts off as Double
    const o = { x: 0.1 };

    // Write to it a few times with setX_Double, to make sure setX_Double has
    // Double feedback.
    setX_Double(o);
    setX_Double(o);

    // Transition o.x to Tagged.
    o.x = {};

    // Write the HeapNumber val to o.x.
    const val = 1.25;
    o.x = val;

    // setX_Double will still have Double feedback, which expects to be able to
    // mutate o.x's HeapNumber, so make sure it does not mutate val.
    setX_Double(o);

    assertEquals(o.x, 4.2);
    assertNotEquals(val, 4.2);
  }

  %EnsureFeedbackVectorForFunction(setX_Double);
  foo();

})();

(function doubleToTaggedWithTaggedValueStoresSmiCorrectly() {

  function setX_Smi(o) { o.x = 42; }

  function foo() {
    // o.x starts off as Double
    const o = { x: 0.1 };

    // Write to it a few times with setX_Smi, to make sure setX_Smi has
    // Double feedback.
    setX_Smi(o);
    setX_Smi(o);

    // Transition o.x to Tagged.
    o.x = {};

    // setX_Smi will still have Double feedback, so make sure it works with
    // the new Tagged representation o.x.
    setX_Smi(o);

    assertEquals(o.x, 42);
  }

  %EnsureFeedbackVectorForFunction(setX_Smi);
  foo();

})();

(function doubleToTaggedWithSmiValueDoesNotMutate() {

  function setX_Smi(o) { o.x = 42; }

  function foo() {
    // o.x starts off as Double
    const o = { x: 0.1 };

    // Write to it a few times with setX_Smi, to make sure setX_Smi has
    // Double feedback.
    setX_Smi(o);
    setX_Smi(o);

    // Transition o.x to Tagged.
    o.x = {};

    // Write the HeapNumber val to o.x.
    const val = 1.25;
    o.x = val;

    // setX_Smi will still have Double feedback, which expects to be able to
    // mutate o.x's HeapNumber, so make sure it does not mutate val.
    setX_Smi(o);

    assertEquals(o.x, 42);
    assertNotEquals(val, 42);
  }

  %EnsureFeedbackVectorForFunction(setX_Smi);
  foo();

})();
