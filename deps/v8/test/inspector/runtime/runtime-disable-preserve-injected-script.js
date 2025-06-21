// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function test() {
  let {session, contextGroup, Protocol} =
      InspectorTest.start(
          "Tests that Runtime.disable doesn't invalidate injected-script.");
  Protocol.Runtime.enable();
  let {result:{result:{objectId}}} = await Protocol.Runtime.evaluate({
    expression: "({a:1})"
  });
  var {result:{result}} = await Protocol.Runtime.getProperties({objectId});
  InspectorTest.logMessage(result.find(property => property.name === 'a'));
  InspectorTest.log('Disabling agent..');
  await Protocol.Runtime.disable();
  var result = await Protocol.Runtime.getProperties({objectId});
  if (result.error) {
    InspectorTest.logMessage(result);
  } else {
    var props = result.result.result;
    InspectorTest.logMessage(props.find(property => property.name === 'a'));
  }
  InspectorTest.completeTest();
})()
