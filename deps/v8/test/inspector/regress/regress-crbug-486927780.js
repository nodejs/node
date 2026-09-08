// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests that destroying context from inside of console.log does not crash');

const expression = `
  Error.prepareStackTrace = function(error, trace) {
    inspector.fireContextDestroyed();
    return '<mock formatted stack trace>';
  };
  console.log(new Error('trigger'));
`;

(async () => {
  Protocol.Runtime.enable();
  contextGroup.createContext('mock-iframe');
  const {params: {context: {uniqueId}}} =
      await Protocol.Runtime.onceExecutionContextCreated();

  await Protocol.Runtime.evaluate({expression, uniqueContextId: uniqueId});

  InspectorTest.completeTest();
})();
