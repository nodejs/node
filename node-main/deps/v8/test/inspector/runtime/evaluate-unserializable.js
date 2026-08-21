// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} =
    InspectorTest.start("Tests Runtime.evaluate with unserializable results.");

Protocol.Runtime.enable();
(async function() {
  await testCase("-0");
  await testCase("NaN");
  await testCase("Infinity");
  await testCase("-Infinity");
  await testCase("1n");

  InspectorTest.completeTest();
})();

async function testCase(expression) {
  InspectorTest.log(expression);
  InspectorTest.logMessage(await Protocol.Runtime.evaluate({expression}));
}
