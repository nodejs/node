// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests for max async call stack depth changed.');

(async function test(){
  utils.setLogMaxAsyncCallStackDepthChanged(true);
  await Protocol.Debugger.enable();
  await Protocol.Debugger.setAsyncCallStackDepth({maxDepth: 8});
  await Protocol.Debugger.setAsyncCallStackDepth({maxDepth: 0});
  await Protocol.Debugger.setAsyncCallStackDepth({maxDepth: 8});
  await Protocol.Debugger.disable();
  InspectorTest.completeTest();
})();
