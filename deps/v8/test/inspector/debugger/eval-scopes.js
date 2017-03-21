// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

InspectorTest.addScript(
`function testNonEmptyEvalScope() {
  eval("'use strict'; var hest = 420; debugger;");
}
function testEmptyEvalScope() {
  eval("var fisk = 42; testNonEmptyEvalScope();");
}`);

Protocol.Debugger.enable();
Protocol.Debugger.oncePaused().then(dumpScopeOnPause);
Protocol.Runtime.evaluate({ "expression": "testEmptyEvalScope();" });

var waitScopeObjects = 0;
function dumpScopeOnPause(message)
{
  var scopeChain = message.params.callFrames[0].scopeChain;
  var evalScopeObjectIds = [];
  for (var scope of scopeChain) {
    if (scope.type === "eval") {
      evalScopeObjectIds.push(scope.object.objectId);
    }
  }
  waitScopeObjects = evalScopeObjectIds.length;
  if (!waitScopeObjects) {
    InspectorTest.completeTest();
  } else {
    for (var objectId of evalScopeObjectIds)
      Protocol.Runtime.getProperties({ "objectId" : objectId })
          .then(dumpProperties);
  }
}

function dumpProperties(message)
{
  InspectorTest.logMessage(message);
  --waitScopeObjects;
  if (!waitScopeObjects)
    Protocol.Debugger.resume().then(InspectorTest.completeTest);
}
