// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var ran = false;

async function test() {
  try {
    let [namespace1, namespace2] = await Promise.all([
      import('modules-skip-1.mjs'),
      import('modules-skip-3.mjs')
    ]);

    let life = namespace1.life();
    let stringlife = namespace2.stringlife;
    assertEquals(42, life);
    assertEquals("42", stringlife);
    ran = true;
  } catch(e) {
    %AbortJS("failure: " + e);
  }
}

test();

%PerformMicrotaskCheckpoint();
assertTrue(ran);
