// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

class Klass {
  #field = 1;
  #method() {}
  get #getter() { return 2; }
  set #setter(v) {}
  static #staticField = 3;
  static #staticMethod() {}
}

class SubWithConflict extends Klass {
  #field = 4;
}

globalThis.Klass = Klass;
globalThis.obj = new Klass();
globalThis.conflictObj = new SubWithConflict();
globalThis.res = undefined;

async function evalREPL(expr) {
  await %RuntimeEvaluateREPL(`res = (${expr})`);
  return globalThis.res;
}

async function test() {
  assertEquals(false, await evalREPL('#brand in globalThis'));
  assertEquals(true, await evalREPL('#field in obj'));
  assertEquals(true, await evalREPL('#method in obj'));
  assertEquals(true, await evalREPL('#getter in obj'));
  assertEquals(true, await evalREPL('#setter in obj'));
  assertEquals(false, await evalREPL('#staticField in obj'));
  assertEquals(true, await evalREPL('#staticField in Klass'));
  assertEquals(true, await evalREPL('#staticMethod in Klass'));

  await assertThrowsAsync(%RuntimeEvaluateREPL('#field in 1'), TypeError);
  await assertThrowsAsync(%RuntimeEvaluateREPL('#field in conflictObj'), Error);
}

assertPromiseResult(test());
