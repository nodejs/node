// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests that console.profile silently fails when there is no source location.');

(async function() {
  Protocol.Profiler.onConsoleProfileStarted(
      message => InspectorTest.logMessage(message));
  await Protocol.Profiler.enable();
  await Protocol.Debugger.enable();
  await Protocol.Runtime.evaluate({
    expression: `
      // Bind console.profile to console to ensure 'this' is correct
      const profile = console.profile.bind(console);
      // Schedule execution directly from event loop to have 0 JS frames
      setTimeout(profile, 0);
    `,
  });
  await InspectorTest.waitForPendingTasks();
  InspectorTest.completeTest();
})();
