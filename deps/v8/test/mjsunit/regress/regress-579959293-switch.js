// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function getScopeObject(gen, index = 0) {
  // clang-format off
  const details = %GetGeneratorScopeDetails(gen, index);
  // clang-format on
  return details ? details[1] : undefined;
}

// 1. Initialized undefined switch local is not in TDZ.
{
  function* genSwitch(v) {
    switch (v) {
      case 1:
        let x = undefined;
        yield 1;
        return x;
    }
  }

  const g = genSwitch(1);
  assertEquals(1, g.next().value);

  const scopeObj = getScopeObject(g, 0);
  assertNotNull(scopeObj);
  // Reading x should return undefined, NOT throw ReferenceError.
  assertEquals(undefined, scopeObj.x);
  assertEquals({value: undefined, done: true}, g.next());
}

// 2. Skipping lexical declaration leaves switch local in TDZ (TdzHole).
{
  function* genSwitchSkipped(v) {
    switch (v) {
      case 1:
        let x = 1;
        yield 1;
        return x;
      case 2:
        yield 2;
        break;
    }
  }

  const g = genSwitchSkipped(2);
  assertEquals(2, g.next().value);

  const scopeObj = getScopeObject(g, 0);
  assertNotNull(scopeObj);
  // Reading x should throw ReferenceError because it was not initialized.
  assertThrows(() => scopeObj.x, ReferenceError);
  assertEquals({value: undefined, done: true}, g.next());
}

// 2b. Switch lexical local with no JS read references (only inspected in
// debugger) still forces hole initialization at switch entry.
{
  function* genSwitchNoRead(v) {
    switch (v) {
      case 1:
        let x = undefined;
        yield 1;
        break;
      case 2:
        yield 2;
        break;
    }
  }

  const g1 = genSwitchNoRead(1);
  assertEquals(1, g1.next().value);
  assertEquals(undefined, getScopeObject(g1, 0).x);
  assertEquals({value: undefined, done: true}, g1.next());

  const g2 = genSwitchNoRead(2);
  assertEquals(2, g2.next().value);
  assertThrows(() => getScopeObject(g2, 0).x, ReferenceError);
  assertEquals({value: undefined, done: true}, g2.next());
}

// 3. Multi-binding destructuring declaration:
// While paused mid-declaration, earlier bindings initialized to undefined
// conservatively appear in TDZ (working as intended).
// Once declaration completes, they are accessible.
{
  function* genDestructuring() {
    let [a, b = (yield 1, 2)] = [undefined];
    yield 2;
    return [a, b];
  }

  const g = genDestructuring();
  // First yield: paused inside default value expression for 'b'.
  assertEquals(1, g.next().value);

  const scopeObjMid = getScopeObject(g, 0);
  assertNotNull(scopeObjMid);
  // Reading 'a' throws ReferenceError because the declaration hasn't finished
  // yet.
  assertThrows(() => scopeObjMid.a, ReferenceError);

  // Second yield: declaration has finished.
  assertEquals(2, g.next().value);

  const scopeObjDone = getScopeObject(g, 0);
  assertNotNull(scopeObjDone);
  // Now 'a' is accessible and undefined.
  assertEquals(undefined, scopeObjDone.a);
  assertEquals({value: [undefined, 2], done: true}, g.next());
}
