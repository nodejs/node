// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start('Tests that scopes do not report variables with empty names');

contextGroup.addScript(
`function testFunction()
{
    for (var a of [1]) {
        ++a;
        debugger;
    }
}`);

Protocol.Debugger.enable();
Protocol.Debugger.oncePaused().then(dumpScopeOnPause);
Protocol.Runtime.evaluate({ "expression": "testFunction()" });

var waitScopeObjects = 0;
function dumpScopeOnPause(message)
{
  var scopeChain = message.params.callFrames[0].scopeChain;
  var localScopeObjectIds = [];
  for (var scope of scopeChain) {
    if (scope.type === "local")
      localScopeObjectIds.push(scope.object.objectId);
  }
  waitScopeObjects = localScopeObjectIds.length;
  if (!waitScopeObjects) {
    InspectorTest.completeTest();
  } else {
    for (var objectId of localScopeObjectIds)
      Protocol.Runtime.getProperties({ "objectId" : objectId }).then(dumpProperties);
  }
}

function dumpProperties(message)
{
  InspectorTest.logMessage(message);
  --waitScopeObjects;
  if (!waitScopeObjects)
    Protocol.Debugger.resume().then(InspectorTest.completeTest);
}
