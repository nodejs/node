// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests crbug.com/882664.');

contextGroup.addScript(`
function provoke_scaling_prefix() {
  var a = [0];
  a[0]++; a[0]++; a[0]++; a[0]++; a[0]++; a[0]++; a[0]++; a[0]++; a[0]++;
  a[0]++; a[0]++; a[0]++; a[0]++; a[0]++; a[0]++; a[0]++; a[0]++; a[0]++;
  a[0]++; a[0]++; a[0]++; a[0]++; a[0]++; a[0]++; a[0]++; a[0]++; a[0]++;
  a[0]++; a[0]++; a[0]++; a[0]++; a[0]++; a[0]++; a[0]++; a[0]++; a[0]++;
  a[0]++; a[0]++; a[0]++; a[0]++; a[0]++; a[0]++; a[0]++; a[0]++; a[0]++;
  a[0]++; a[0]++; a[0]++; a[0]++; a[0]++; a[0]++; a[0]++; a[0]++; a[0]++;
  a[0]++; a[0]++; a[0]++; a[0]++; a[0]++; a[0]++; a[0]++; a[0]++; a[0]++;
  a[0]++; a[0]++; a[0]++; a[0]++; a[0]++; a[0]++; a[0]++; a[0]++; a[0]++;
  a[0]++; a[0]++; a[0]++; a[0]++; a[0]++; a[0]++; a[0]++; a[0]++; a[0]++;
  a[0]++; a[0]++; a[0]++; a[0]++; a[0]++; a[0]++; a[0]++; a[0]++; a[0]++;
  a[0]++; a[0]++; a[0]++; a[0]++; a[0]++; a[0]++; a[0]++; a[0]++; a[0]++;
  a[0]++; a[0]++; a[0]++; a[0]++; a[0]++; a[0]++; a[0]++; a[0]++; a[0]++;
  a[0]++; a[0]++; a[0]++; a[0]++; a[0]++; a[0]++; a[0]++; a[0]++; a[0]++;
  a[0]++; a[0]++; a[0]++; a[0]++; a[0]++; a[0]++; a[0]++; a[0]++; a[0]++;
  a[0]++; a[0]++; a[0]++; a[0]++; a[0]++; a[0]++; a[0]++; a[0]++; a[0]++;
  a[0]++; a[0]++; a[0]++; a[0]++; a[0]++; a[0]++; a[0]++; a[0]++; a[0]++;
  a[0]++; a[0]++; a[0]++; a[0]++; a[0]++; a[0]++; a[0]++; a[0]++; a[0]++;
  a[0]++; a[0]++; a[0]++; a[0]++; a[0]++; a[0]++; a[0]++; a[0]++; a[0]++;
  a[0]++; a[0]++; a[0]++; a[0]++; a[0]++; a[0]++; a[0]++; a[0]++; a[0]++;
  a[0]++; a[0]++; a[0]++; a[0]++; a[0]++; a[0]++; a[0]++; a[0]++; a[0]++;
  a[0]++; a[0]++; a[0]++; a[0]++; a[0]++; a[0]++; a[0]++; a[0]++; a[0]++;
  a[0]++; a[0]++; a[0]++; a[0]++; a[0]++; a[0]++; a[0]++; a[0]++; a[0]++;
  a[0]++; a[0]++; a[0]++; a[0]++; a[0]++; a[0]++; a[0]++; a[0]++; a[0]++;
  a[0]++; a[0]++; a[0]++; a[0]++; a[0]++; a[0]++; a[0]++; a[0]++; a[0]++;
  return a;
}
`);

(async function test() {
  await Protocol.Runtime.enable();
  const {result: {result}} = await Protocol.Runtime.evaluate({
    expression: 'provoke_scaling_prefix()',
    throwOnSideEffect: true,
    returnByValue: true,
  });
  InspectorTest.logMessage(result);
  InspectorTest.completeTest();
})();
