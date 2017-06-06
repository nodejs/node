// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

InspectorTest.log('Checks createContext().');

InspectorTest.setupScriptMap();
Protocol.Runtime.onExecutionContextCreated(InspectorTest.logMessage);
Protocol.Debugger.onPaused((message) => {
  InspectorTest.logSourceLocation(message.params.callFrames[0].location);
  Protocol.Debugger.stepOut();
});
var executionContextIds = new Set();
Protocol.Debugger.onScriptParsed(message => executionContextIds.add(message.params.executionContextId));
var contextGroupId;
Protocol.Runtime.enable()
  .then(() => contextGroupId = utils.createContextGroup())
  .then(() => Protocol.Runtime.enable({}, contextGroupId))
  .then(() => Protocol.Debugger.enable())
  .then(() => Protocol.Debugger.enable({}, contextGroupId))
  .then(InspectorTest.logMessage)
  .then(() => {
    Protocol.Runtime.evaluate({ expression: 'debugger;' })
    Protocol.Runtime.evaluate({ expression: 'setTimeout(x => x * 2, 0)' }, contextGroupId);
    Protocol.Runtime.evaluate({ expression: 'setTimeout(x => x * 3, 0)' });
  })
  .then(() => InspectorTest.waitPendingTasks())
  .then(() => {
    InspectorTest.log(`Reported script's execution id: ${executionContextIds.size}`);
    executionContextIds.clear();
  })
  .then(() => utils.reconnect())
  .then(() => {
    Protocol.Runtime.evaluate({ expression: 'debugger;' })
    Protocol.Runtime.evaluate({ expression: 'setTimeout(x => x * 2, 0)' }, contextGroupId);
    Protocol.Runtime.evaluate({ expression: 'setTimeout(x => x * 3, 0)' });
  })
  .then(() => InspectorTest.waitPendingTasks())
  .then(() => Protocol.Debugger.disable({}, contextGroupId))
  .then(() => Protocol.Debugger.disable({}))
  .then(() => InspectorTest.log(`Reported script's execution id: ${executionContextIds.size}`))
  .then(InspectorTest.completeTest);
