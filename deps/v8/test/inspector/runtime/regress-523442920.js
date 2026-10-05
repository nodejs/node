// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} =
  InspectorTest.start('Tests that destroying context during inspect does not cause UAF (regress-523442920).');

(async function test() {
  await Protocol.Runtime.enable();

  // Capture CommandLineAPI inspect function.
  await Protocol.Runtime.evaluate({
    expression: 'globalThis.savedInspect = inspect;',
    includeCommandLineAPI: true,
  });

  // Call savedInspect from page-level execution where no ContextScope is active.
  contextGroup.addScript(`
    let e = new Error();
    delete e.name;
    Object.defineProperty(Object.getPrototypeOf(e), 'name', {
      get() {
        inspector.fireContextDestroyed();
        return '';
      }
    });
    savedInspect(e);
  `);

  InspectorTest.log('Success (no crash).');
  InspectorTest.completeTest();
})();
