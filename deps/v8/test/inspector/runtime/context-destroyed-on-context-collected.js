// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-stress-incremental-marking

let {session, contextGroup, Protocol} =
  InspectorTest.start('Tests that contextDesrtoyed nofitication is fired when context is collected.');

(async function test() {
  await Protocol.Runtime.enable();
  Protocol.Runtime.onExecutionContextDestroyed(InspectorTest.logMessage);
  contextGroup.addScript('inspector.freeContext()');
  await Protocol.HeapProfiler.collectGarbage();
  InspectorTest.completeTest();
})();
