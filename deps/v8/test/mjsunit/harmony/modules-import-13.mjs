// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --harmony-dynamic-import
// Resources: test/mjsunit/harmony/modules-skip-1.mjs

let ran = false;
async function test1() {
  try {
    let x = { toString() { return 'modules-skip-1.mjs' } };
    let namespace = await import(x);
    let life = namespace.life();
    assertEquals(42, life);
    ran = true;
  } catch(e) {
    %AbortJS('failure: '+ e);
  }
}

test1();
%PerformMicrotaskCheckpoint();
assertTrue(ran);

ran = false;
async function test2() {
  try {
    let x = { get toString() { return () => 'modules-skip-1.mjs' } };
    let namespace = await import(x);
    let life = namespace.life();
    assertEquals(42, life);
    ran = true;
  } catch(e) {
    %AbortJS('failure: '+ e);
  }
}

test2();
%PerformMicrotaskCheckpoint();
assertTrue(ran);
