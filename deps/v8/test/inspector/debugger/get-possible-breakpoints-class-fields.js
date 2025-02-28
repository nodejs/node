// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let { session, contextGroup, Protocol } = InspectorTest.start(
  "Checks Debugger.getPossibleBreakpoints for class fields"
);

(async function() {
  session.setupScriptMap();
  await Protocol.Debugger.enable();

  const source = utils.read(
    "test/inspector/debugger/resources/break-locations-class-fields.js"
  );

  contextGroup.addScript(source);

  const {
    params: { scriptId }
  } = await Protocol.Debugger.onceScriptParsed();

  const {
    result: { locations }
  } = await Protocol.Debugger.getPossibleBreakpoints({
    start: {
      lineNumber: 0,
      columnNumber: 0,
      scriptId
    }
  });

  session.logBreakLocations(locations);
  InspectorTest.completeTest();
})();
