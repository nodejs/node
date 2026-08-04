// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {Protocol, contextGroup} = InspectorTest.start(
  `Tests how Runtime.evaluate handles uniqueContextId argument`);

(async function test(){
  Protocol.Runtime.enable();
  const context1 = (await Protocol.Runtime.onceExecutionContextCreated()).params.context;

  contextGroup.createContext();
  const context2 = (await Protocol.Runtime.onceExecutionContextCreated()).params.context;

  Protocol.Runtime.evaluate({
    expression: 'token = "context 1";',
    contextId: context1.id
  });
  Protocol.Runtime.evaluate({
    expression: 'token = "context 2";',
    contextId: context2.id
  });

  {
    const response = (await Protocol.Runtime.evaluate({
      expression: 'token',
      uniqueContextId: context1.uniqueId,
      returnByValue: true
    })).result.result.value;
    InspectorTest.logMessage(`token in context 1: ${response}`);
  }
  {
    const response = (await Protocol.Runtime.evaluate({
      expression: 'token',
      uniqueContextId: context2.uniqueId,
      returnByValue: true
    })).result.result.value;
    InspectorTest.logMessage(`token in context 2: ${response}`);
  }

  // The following tests are for error handling.
  {
    const response = (await Protocol.Runtime.evaluate({
      expression: 'token',
      uniqueContextId: context1.uniqueId,
      contextId: context1.id
    }));
    InspectorTest.logMessage(response);
  }
  {
    const response = (await Protocol.Runtime.evaluate({
      expression: 'token',
      uniqueContextId: 'fubar',
    }));
    InspectorTest.logMessage(response);
  }
  {
    const response = (await Protocol.Runtime.evaluate({
      expression: 'token',
      uniqueContextId: context1.uniqueId + 1,
    }));
    InspectorTest.logMessage(response);
  }

  InspectorTest.completeTest();
})();
