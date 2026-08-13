// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests Runtime.evaluate returns object with undefined property.');

(async function test() {
  InspectorTest.logMessage('CommandLineAPI can be overridden by Object.defineProperty:')
  InspectorTest.logMessage((await Protocol.Runtime.evaluate({
    expression: 'Object.defineProperty(globalThis, "copy", { get() { return 239; }}); copy;',
    includeCommandLineAPI: true,
  })).result.result);
  InspectorTest.logMessage('CommandLineAPI doesn\'t override Object.defineProperty:')
  InspectorTest.logMessage((await Protocol.Runtime.evaluate({
    expression: 'copy',
    includeCommandLineAPI: true,
  })).result.result);
  InspectorTest.completeTest();
})();
