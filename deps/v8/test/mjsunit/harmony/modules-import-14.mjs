// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var ran = false;

async function test() {
  try {
    let x = await import('modules-skip-1.mjs');
    // modules-skip-5.mjs statically imports modules-skip-1.mjs
    let y = await import('modules-skip-5.mjs');
    assertSame(x, y.static_life);

    let z = await import('modules-skip-1.mjs');
    assertSame(x, z);
    ran = true;
  } catch(e) {
    %AbortJS('failure: '+ e.message);
  }
}

test();
%PerformMicrotaskCheckpoint();
assertTrue(ran);
