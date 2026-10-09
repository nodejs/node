// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {contextGroup, Protocol} = InspectorTest.start(
    'Tests that console.assert does not crash when the context has no contextGroupId');

InspectorTest.runAsyncTestSuite([
  async function testAssertInDestroyedContext() {
    await Protocol.Runtime.enable();
    await Protocol.Debugger.enable();
    await Protocol.Debugger.setPauseOnExceptions({state: 'all'});

    contextGroup.createContext('destroyed-before-assert');
    const {params: {context: {uniqueId}}} =
        await Protocol.Runtime.onceExecutionContextCreated();

    await Protocol.Runtime.evaluate({
      expression: `
        inspector.fireContextDestroyed();
        console.assert(false);
      `,
      uniqueContextId: uniqueId,
    });

    await Protocol.Debugger.disable();
    await Protocol.Runtime.disable();
  },

  async function testDestroyContextDuringAssertFormatting() {
    await Protocol.Runtime.enable();
    await Protocol.Debugger.enable();
    await Protocol.Debugger.setPauseOnExceptions({state: 'all'});

    contextGroup.createContext('destroyed-during-formatting');
    const {params: {context: {uniqueId}}} =
        await Protocol.Runtime.onceExecutionContextCreated();

    await Protocol.Runtime.evaluate({
      expression: `
        const err = new Error('trigger');
        Object.defineProperty(err, 'name', {
          get() {
            inspector.fireContextDestroyed();
            return 'Error';
          },
        });
        console.assert(false, err);
      `,
      uniqueContextId: uniqueId,
    });

    await Protocol.Debugger.disable();
    await Protocol.Runtime.disable();
  },

  async function testDestroyContextDuringAssertWrapping() {
    await Protocol.Runtime.enable();
    await Protocol.Debugger.enable();
    await Protocol.Debugger.setPauseOnExceptions({state: 'all'});

    contextGroup.createContext('destroyed-during-wrapping');
    const {params: {context: {uniqueId}}} =
        await Protocol.Runtime.onceExecutionContextCreated();

    await Protocol.Runtime.evaluate({
      expression: `
        Error.prepareStackTrace = function(error, trace) {
          inspector.fireContextDestroyed();
          return '<mock formatted stack trace>';
        };
        console.assert(false, new Error('trigger'));
      `,
      uniqueContextId: uniqueId,
    });

    await Protocol.Debugger.disable();
    await Protocol.Runtime.disable();
  },
]);
