// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

InspectorTest.log('Checks createContext().');

var executionContextIds = new Set();
var contextGroup1 = new InspectorTest.ContextGroup();
var session1 = contextGroup1.connect();
setup(session1);
var contextGroup2 = new InspectorTest.ContextGroup();
var session2 = contextGroup2.connect();
setup(session2);

session1.Protocol.Runtime.enable()
  .then(() => session2.Protocol.Runtime.enable({}))
  .then(() => session1.Protocol.Debugger.enable())
  .then(() => session2.Protocol.Debugger.enable({}))
  .then(InspectorTest.logMessage)
  .then(() => {
    session1.Protocol.Runtime.evaluate({ expression: 'debugger;' });
    session2.Protocol.Runtime.evaluate({expression: 'setTimeout(x => x * 2, 0)'});
    session1.Protocol.Runtime.evaluate({ expression: 'setTimeout(x => x * 3, 0)' });
  })
  .then(() => InspectorTest.waitForPendingTasks())
  .then(() => {
    InspectorTest.log(`Reported script's execution id: ${executionContextIds.size}`);
    executionContextIds.clear();
  })
  .then(() => session1.reconnect())
  .then(() => session2.reconnect())
  .then(() => {
    session1.Protocol.Runtime.evaluate({ expression: 'debugger;' })
    session2.Protocol.Runtime.evaluate({ expression: 'setTimeout(x => x * 2, 0)' });
    session1.Protocol.Runtime.evaluate({ expression: 'setTimeout(x => x * 3, 0)' });
  })
  .then(() => InspectorTest.waitForPendingTasks())
  .then(() => session2.Protocol.Debugger.disable({}))
  .then(() => session1.Protocol.Debugger.disable({}))
  .then(() => InspectorTest.log(`Reported script's execution id: ${executionContextIds.size}`))
  .then(InspectorTest.completeTest);

function setup(session) {
  session.Protocol.Runtime.onExecutionContextCreated(InspectorTest.logMessage);
  session.setupScriptMap();
  session.Protocol.Debugger.onPaused((message) => {
    session.logSourceLocation(message.params.callFrames[0].location);
    session.Protocol.Debugger.stepOut();
  });
  session.Protocol.Debugger.onScriptParsed(message => executionContextIds.add(message.params.executionContextId));
}
