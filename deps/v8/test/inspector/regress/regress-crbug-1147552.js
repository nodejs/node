// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, Protocol} = InspectorTest.start('Regression test for crbug.com/1147552. Found by Clusterfuzz.');

Protocol.Runtime.enable();

Protocol.Runtime.setAsyncCallStackDepth({maxDepth: 10});
(async function test() {
  await Protocol.Runtime.setMaxCallStackSizeToCapture({size: 0});
  await Protocol.Runtime.evaluate({ expression: 'foo'});

  InspectorTest.log('Test must not have crashed.')
  InspectorTest.completeTest();
})();
