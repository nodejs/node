// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start("Test that Runtime.getProperties doesn't truncate set and map entries in internalProperties.")

contextGroup.addScript(`
  function createSet(size) {
    var s = new Set();
    var a = {};
    a.a = a;
    for (var i = 0; i < size; ++i) s.add({ wrapper: a});
    return s;
  }

  function createMap(size) {
    var m = new Map();
    var a = {};
    a.a = a;
    for (var i = 0; i < size; ++i) m.set(i, { wrapper: a});
    return m;
  }
`);

contextGroup.setupInjectedScriptEnvironment();

Protocol.Debugger.enable();
Protocol.Runtime.enable();

testExpression("createSet(10)")
  .then(() => testExpression("createSet(1000)"))
  .then(() => testExpression("createMap(10)"))
  .then(() => testExpression("createMap(1000)"))
  .then(() => InspectorTest.completeTest());

function testExpression(expression)
{
  return Protocol.Runtime.evaluate({ "expression": expression})
           .then(result => Protocol.Runtime.getProperties({ ownProperties: true, objectId: result.result.result.objectId }))
           .then(message => dumpEntriesDescription(expression, message));
}

function dumpEntriesDescription(expression, message)
{
  InspectorTest.log(`Entries for "${expression}"`);
  var properties = message.result.internalProperties;
  var property;
  if (properties)
    property = properties.find(property => property.name === "[[Entries]]");
  if (!property)
    InspectorTest.log("[[Entries]] not found");
  else
    InspectorTest.log(property.value.description);
}
