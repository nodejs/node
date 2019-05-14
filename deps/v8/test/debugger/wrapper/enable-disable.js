// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let compileCount = 0;

const Debug = new DebugWrapper();

Debug.setListener(function(event, exec_state, event_data, data) {
  if (event != Debug.DebugEvent.AfterCompile) return;
  compileCount++;
});

Debug.enable();
assertTrue(compileCount != 0);

const compileCountAfterEnable = compileCount;

Debug.enable();  // Idempotent.
assertEquals(compileCountAfterEnable, compileCount);

Debug.disable();
assertEquals(compileCountAfterEnable, compileCount);

Debug.disable();  // Idempotent.
assertEquals(compileCountAfterEnable, compileCount);

Debug.enable();  // Re-enabling causes recompilation.
assertEquals(2 * compileCountAfterEnable, compileCount);
