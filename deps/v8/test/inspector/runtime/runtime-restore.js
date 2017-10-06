// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.v8

let {session, contextGroup, Protocol} = InspectorTest.start('Checks that Runtime agent correctly restore its state.');

contextGroup.addScript(`
var formatter = {
    header: function(x)
    {
        return ["span", {}, "Header formatted ", x.name];
    },

    hasBody: function(x)
    {
        return true;
    },

    body: function(x)
    {
        return ["span", {}, "Body formatted ", x.name]
    }
};

devtoolsFormatters = [ formatter ];

//# sourceURL=test.js`)

InspectorTest.runTestSuite([
  function testExecutionContextsNotificationsOnRestore(next) {
    Protocol.Runtime.onExecutionContextsCleared(InspectorTest.logMessage);
    Protocol.Runtime.onExecutionContextCreated(InspectorTest.logMessage);
    Protocol.Runtime.onExecutionContextDestroyed(InspectorTest.logMessage);
    Protocol.Runtime.enable()
      .then(reconnect)
      .then(Protocol.Runtime.disable)
      .then(() => {
        Protocol.Runtime.onExecutionContextsCleared(null);
        Protocol.Runtime.onExecutionContextCreated(null);
        Protocol.Runtime.onExecutionContextDestroyed(null);
        next()
      });
  },

  function testConsoleAPICalledAfterRestore(next) {
    Protocol.Runtime.onConsoleAPICalled(InspectorTest.logMessage);
    Protocol.Runtime.enable()
      .then(reconnect)
      .then(() => Protocol.Runtime.evaluate({ expression: 'console.log(42);' }))
      .then(Protocol.Runtime.disable)
      .then(() => {
        Protocol.Runtime.onConsoleAPICalled(null);
        next();
      });
  },

  function testSetCustomObjectFormatterEnabled(next) {
    Protocol.Runtime.onConsoleAPICalled(InspectorTest.logMessage);
    Protocol.Runtime.discardConsoleEntries()
      .then(reconnect)
      .then(() => Protocol.Runtime.enable())
      .then(() => Protocol.Runtime.setCustomObjectFormatterEnabled({ enabled: true }))
      .then(reconnect)
      .then(() => Protocol.Runtime.evaluate({ expression: 'console.log({ name: 42 })'}))
      .then(InspectorTest.logMessage)
      .then(Protocol.Runtime.disable)
      .then(() => {
        Protocol.Runtime.onConsoleAPICalled(null);
        next();
      });
  },
]);

function reconnect() {
  InspectorTest.logMessage('will reconnect..');
  session.reconnect();
}
