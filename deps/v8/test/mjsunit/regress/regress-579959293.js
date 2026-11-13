// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Case 1: Switch scopes
// Mutating an initialized switch local with undefined succeeds (42), and
// reading x after that mutation (and upon resume) observes 42.
{
  function* testSwitchInitialized() {
    switch (1) {
      case 1:
        let x = undefined;
        yield 1;
        return x;
    }
  }

  const g1 = testSwitchInitialized();
  assertEquals({value: 1, done: false}, g1.next());

  // Mutating x from undefined to 42 succeeds:
  const setResult1 = %SetGeneratorScopeVariableValue(g1, 0, 'x', 42);
  assertTrue(setResult1);

  // Reading x after mutation observes the mutated value:
  const details1 = %GetGeneratorScopeDetails(g1, 0);
  assertEquals(42, details1[1].x);
  assertEquals({value: 42, done: true}, g1.next());
}

// Case 1 (continued): Uninitialized switch local (holding TdzHole).
// Jumping over `case 1: let x = 1;` leaves `x` in true TDZ (holding TdzHole).
// Mutating x via %SetGeneratorScopeVariableValue must be rejected.
{
  function* testSwitchUninitialized() {
    switch (0) {
      case 1:
        let x = 1;
        return x;
      default:
        yield 1;
    }
  }

  const g2 = testSwitchUninitialized();
  assertEquals({value: 1, done: false}, g2.next());
  const setResult2 = %SetGeneratorScopeVariableValue(g2, 0, 'x', 42);
  assertFalse(setResult2);
}

// Case 2: Multi-binding destructuring declaration
// Mutating an already-initialized undefined binding in a multi-binding
// destructuring declaration while paused inside b's initializer succeeds (40),
// and resuming returns [40, 2].
{
  function* testDestructuringArray() {
    let [a, b = (yield 1, 2)] = [undefined];
    return [a, b];
  }

  const g3 = testDestructuringArray();
  assertEquals({value: 1, done: false}, g3.next());

  // Mutating a mid-declaration succeeds:
  const setResult3 = %SetGeneratorScopeVariableValue(g3, 0, 'a', 40);
  assertTrue(setResult3);

  // Resuming observes the mutated value:
  assertEquals({value: [40, 2], done: true}, g3.next());
}

{
  function* testDestructuringObject() {
    let {a = undefined, b = (yield 1, 2)} = {};
    return [a, b];
  }

  const g4 = testDestructuringObject();
  assertEquals({value: 1, done: false}, g4.next());

  const setResult4 = %SetGeneratorScopeVariableValue(g4, 0, 'a', 40);
  assertTrue(setResult4);

  assertEquals({value: [40, 2], done: true}, g4.next());
}

// Case 3: Elided-TDZ lexical local before its initializer.
// Writing via %SetGeneratorScopeVariableValue to an elided-TDZ local before
// its initializer runs is permitted as working-as-intended (overwriting the
// dead register without safety hazard, even though a debugger read before
// resume can observe the written value) and is overwritten by the initializer
// upon resume.
{
  function* testLexicalVarElidedTdz() {
    let x = (yield 0, 7);
    return x;
  }

  const g5 = testLexicalVarElidedTdz();
  assertEquals({value: 0, done: false}, g5.next());

  // SetVariableValue writes to the dead register:
  const setResult5 = %SetGeneratorScopeVariableValue(g5, 0, 'x', 99);
  assertTrue(setResult5);
  // Debugger read can observe the written value (WAI):
  const details5 = %GetGeneratorScopeDetails(g5, 0);
  assertEquals(99, details5[1].x);
  // Initializer runs upon resume and overwrites it with 7:
  assertEquals({value: 7, done: true}, g5.next());
}

{
  function* testDestructuringArrayElidedTdz() {
    let [a, b] = (yield 0, [undefined, 2]);
    return [a, b];
  }

  const g6 = testDestructuringArrayElidedTdz();
  assertEquals({value: 0, done: false}, g6.next());

  const setResult6 = %SetGeneratorScopeVariableValue(g6, 0, 'a', 99);
  assertTrue(setResult6);
  // Initializer runs upon resume and overwrites it with [undefined, 2]:
  assertEquals({value: [undefined, 2], done: true}, g6.next());
}
