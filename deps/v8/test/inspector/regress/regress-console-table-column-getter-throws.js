// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} = InspectorTest.start(
    'Checks that a throwing console.table() column getter does not leak the ' +
    'exception into the running script.');

const expression = `
  (function() {
    let count = 0;
    const columns = [];
    Object.defineProperty(columns, 0, {
      get() {
        // The first invocation happens in the user's own call; the second one
        // comes from the inspector re-reading the columns in wrapTable().
        if (++count === 2) throw new Error('boom');
        return 'a';
      },
      enumerable: true,
      configurable: true,
    });
    try {
      console.table([{a: 1}], columns);
      return 'no throw';
    } catch (e) {
      return 'threw ' + e.message;
    }
  })()
`;

(async () => {
  const before = await Protocol.Runtime.evaluate({expression});
  InspectorTest.log(`Runtime disabled: ${before.result.result.value}`);

  await Protocol.Runtime.enable();

  const after = await Protocol.Runtime.evaluate({expression});
  InspectorTest.log(`Runtime enabled: ${after.result.result.value}`);

  InspectorTest.completeTest();
})();
