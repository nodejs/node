// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} =
  InspectorTest.start('Tests that addBinding is safe against context destruction during execution (regress-507508114).');

(async function test() {
  await Protocol.Runtime.enable();

  // Create a custom prototype setter in the inspected context.
  contextGroup.addScript(`
    Object.defineProperty(Object.prototype, 'binding', {
      configurable: true,
      set: function(v) {
        // Synchronously destroy the context from inside the setter
        inspector.fireContextDestroyed();
      }
    });
  `);

  InspectorTest.log('Calling addBinding (should not crash)...');
  await Protocol.Runtime.addBinding({name: 'binding'});

  InspectorTest.log('Success (no crash).');
  InspectorTest.completeTest();
})();
