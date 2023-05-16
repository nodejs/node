// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const { Protocol, contextGroup } = InspectorTest.start('Don\'t crash when injected script dies during Promise.then');

(async () => {
  // Overwrite 'Promise#then' to block.
  contextGroup.addScript(`
    Object.prototype.__defineGetter__('then', function () {
      inspector.runNestedMessageLoop(); // Doesn't return.
    });
  `);

  // Trigger an evaluation that installs the inspector promise handler. Note
  // that the expression is somewhat carefully crafted so we stop in the right
  // micro task.
  Protocol.Runtime.evaluate({
    expression: `(() => ({ foo: 42 }))()`,
    awaitPromise: true,
  });

  contextGroup.reset();

  InspectorTest.completeTest();
})();
