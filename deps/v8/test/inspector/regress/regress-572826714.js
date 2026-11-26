// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --flag-processing-mode=ignore-contradictions --cet-compatible
// Flags: --always-sparkplug

const {session, contextGroup, Protocol} =
    InspectorTest.start('Regression test for crbug.com/572826714.');

contextGroup.addScript(`
function foo(depth) {
  if (depth > 0) {
    return foo(depth - 1);
  }
  debugger;
}
`);

(async () => {
  await Protocol.Debugger.enable();
  Protocol.Debugger.onPaused(async () => {
    await Protocol.Debugger.stepOver();
  });
  await Protocol.Runtime.evaluate({expression: 'foo(2)'});
  InspectorTest.completeTest();
})();
