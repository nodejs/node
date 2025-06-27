// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var ran = false;

async function test() {
  try {
    let namespace = await import('modules-skip-4.mjs');
    assertEquals(42, namespace.life());
    assertEquals("42", namespace.stringlife);
    ran = true;
  } catch(e) {
    %AbortJS('failure: '+ e);
  }
}

test();

%PerformMicrotaskCheckpoint();

assertTrue(ran);
