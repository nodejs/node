// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const { contextGroup, Protocol } = InspectorTest.start(
    'Check that setBreakpointByUrl in a broken script doesn\'t trigger scriptFailedToParse');

(async () => {
  Protocol.Debugger.onScriptFailedToParse(InspectorTest.logMessage);
  await Protocol.Debugger.enable();

  contextGroup.addScript('{', 0, 0, 'foo.js');

  await Protocol.Debugger.setBreakpointByUrl({
    lineNumber: 0,
    urlRegex: ".*",
  });

  InspectorTest.completeTest();
})();
