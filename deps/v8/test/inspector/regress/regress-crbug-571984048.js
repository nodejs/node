// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests that console label APIs bail out when the context is destroyed ' +
    'during label conversion.');

InspectorTest.runAsyncTestSuite([async function testLabelConversion() {
  await Protocol.Runtime.enable();
  Protocol.Runtime.onConsoleAPICalled(
      ({params: {type}}) =>
          InspectorTest.log(`Unexpected console API call: ${type}`));

  for (const method of ['count', 'countReset', 'time', 'timeLog',
                        'timeEnd']) {
    contextGroup.createContext(`destroyed-context-${method}`);
    const {params: {context: {uniqueId}}} =
        await Protocol.Runtime.onceExecutionContextCreated();

    const {result, error} = await Protocol.Runtime.evaluate({
      expression: `
        console.${method}({
          toString() {
            inspector.fireContextDestroyed();
            return 'label';
          }
        });
        'done';
      `,
      uniqueContextId: uniqueId,
    });
    InspectorTest.log(`console.${method}(): ${
        error ? `error: ${error.message}` : result.result.value}`);
  }

  await Protocol.Runtime.disable();
}]);
