// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start('Tests that suspended generators produce scopes');

contextGroup.addScript(
`function *gen(a) {
  var b = 42;
  yield a;
  return b;
}
function testSuspendedGenerator()
{
  var g = gen(420);
  g.next();

  debugger;
  return g;
}`);

Protocol.Debugger.enable().then(testSuite);

function dumpInnermostScope(msg) {
  var scopes = msg.result.result;
  var inner_scope = scopes[0].value;
  return Protocol.Runtime.getProperties({ objectId : inner_scope.objectId })
      .then(InspectorTest.logMessage);
}

function dumpGeneratorScopes(msg)
{
  var props = msg.result.internalProperties;
  var promises = props
      .filter(prop => prop.name == "[[Scopes]]")
      .map(prop => prop.value.objectId)
      .map(scopesId => Protocol.Runtime.getProperties({ objectId : scopesId })
          .then(dumpInnermostScope));
  return Promise.all(promises);
}

function fetchGeneratorProperties(objectId) {
  return Protocol.Runtime.getProperties({ objectId : objectId });
}

function extractGeneratorObjectFromScope(scopeId) {
  return Protocol.Runtime.getProperties({ objectId : scopeId })
      .then(msg => {
          var generatorObjectId = msg.result.result[0].value.objectId;
          return fetchGeneratorProperties(generatorObjectId);
      });
}

function dumpGeneratorScopesOnPause(msg) {
  var scopeChain = msg.params.callFrames[0].scopeChain;
  var promises = scopeChain
      .filter(scope => scope.type === "local")
      .map(scope => scope.object.objectId)
      .map(scopeId => extractGeneratorObjectFromScope(scopeId)
                          .then(dumpGeneratorScopes));
  return Promise.all(promises).then(Protocol.Debugger.resume);
}

function testSuite() {
  InspectorTest.runTestSuite([

    function testScopesPaused(next) {
      Protocol.Debugger.oncePaused()
          .then(dumpGeneratorScopesOnPause)
          .then(next);
      Protocol.Runtime.evaluate({ expression : "testSuspendedGenerator()" });
    },

    function testScopesNonPaused(next) {
      Protocol.Runtime.evaluate({ expression : "gen(430)"})
          .then(msg => fetchGeneratorProperties(msg.result.result.objectId))
          .then(dumpGeneratorScopes)
          .then(next);
    },
  ]);
}
